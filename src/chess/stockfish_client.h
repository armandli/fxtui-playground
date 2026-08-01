#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <fcntl.h>
#include <functional>
#include <mutex>
#include <optional>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "uci_protocol.h"

// Set by SpawnEngineProcess's parent branch immediately after fork(), cleared
// once ~StockfishClient() has fully reaped the child. Read by the top-level
// SIGINT/SIGTERM handler in main.cpp (only load-bearing before screen.Loop()
// starts and briefly after it returns -- while the TUI is running, FTXUI's
// own signal handler converts Ctrl+C into a graceful Loop() exit instead).
inline std::atomic<pid_t> g_engine_pid{-1};

// Forks and execs `path` with its stdin/stdout redirected to pipes. Returns
// the child pid (with `in_write_fd`/`out_read_fd` set to the parent's ends of
// the pipes) or -1 on failure.
//
// The child branch must never return/throw/exit()/std::exit() -- only
// _exit() or a successful exec() may end it, to avoid double-flushing
// iostream buffer state inherited from the parent.
inline pid_t SpawnEngineProcess(const std::string& path, int& in_write_fd, int& out_read_fd) {
    int in_pipe[2], out_pipe[2];
    if (pipe(in_pipe) != 0)
        return -1;
    if (pipe(out_pipe) != 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(in_pipe[0], STDIN_FILENO);
        dup2(out_pipe[1], STDOUT_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        char* argv[] = {const_cast<char*>(path.c_str()), nullptr};
        execvp(path.c_str(), argv);
        _exit(127);  // exec failed
    }

    // Parent
    close(in_pipe[0]);
    close(out_pipe[1]);
    in_write_fd = in_pipe[1];
    out_read_fd = out_pipe[0];
    g_engine_pid.store(pid, std::memory_order_relaxed);
    return pid;
}

class StockfishClient {
public:
    enum class GoMode { Movetime, Depth };

    struct DifficultyConfig {
        bool limit_strength = true;
        int elo = 1500;
        GoMode go_mode = GoMode::Movetime;
        int go_value = 1000;
    };

    StockfishClient(std::string engine_path, std::function<void()> on_update)
        : engine_path_(std::move(engine_path)), on_update_(std::move(on_update)) {}

    StockfishClient(const StockfishClient&) = delete;
    StockfishClient& operator=(const StockfishClient&) = delete;

    ~StockfishClient() {
        if (to_engine_) {
            SendLine(CmdQuit());
            std::fclose(to_engine_);
            to_engine_ = nullptr;
        }
        if (pid_ > 0) {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            int status = 0;
            pid_t r = 0;
            while (std::chrono::steady_clock::now() < deadline) {
                r = waitpid(pid_, &status, WNOHANG);
                if (r == pid_ || r == -1)
                    break;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            if (r != pid_) {
                kill(pid_, SIGKILL);
                waitpid(pid_, &status, 0);
            }
            g_engine_pid.store(-1, std::memory_order_relaxed);
            pid_ = -1;
        }
        reader_running_.store(false);
        if (reader_thread_.joinable())
            reader_thread_.join();  // fast: the process is confirmed dead above
        if (from_engine_) {
            std::fclose(from_engine_);
            from_engine_ = nullptr;
        }
    }

    bool Start(const DifficultyConfig& config) {
        config_ = config;
        int in_fd = -1, out_fd = -1;
        pid_ = SpawnEngineProcess(engine_path_, in_fd, out_fd);
        if (pid_ < 0)
            return false;
        to_engine_ = fdopen(in_fd, "w");
        from_engine_ = fdopen(out_fd, "r");
        if (!to_engine_ || !from_engine_)
            return false;

        reader_running_.store(true);
        reader_thread_ = std::thread(&StockfishClient::ReaderLoop, this);

        SendLine(CmdUci());
        if (!WaitFor(uciok_received_, std::chrono::seconds(5)))
            return false;

        SendLine(CmdSetOption("UCI_LimitStrength", config_.limit_strength ? "true" : "false"));
        if (config_.limit_strength)
            SendLine(CmdSetOption("UCI_Elo", std::to_string(config_.elo)));

        SendLine(CmdIsReady());
        if (!WaitFor(readyok_received_, std::chrono::seconds(5)))
            return false;
        return true;
    }

    void NewGame() {
        SendLine(CmdUciNewGame());
        SendLine(CmdIsReady());  // fire-and-forget; the engine processes stdin lines strictly in order
    }

    // Cancels an in-flight search and discards any stale pending result.
    // Call before NewGame() whenever resetting mid-search, otherwise a
    // search already in flight when "ucinewgame" is sent violates the UCI
    // protocol and its eventual bestmove could land on the new game.
    void CancelPending() {
        SendLine(CmdStop());
        {
            std::lock_guard<std::mutex> lock(mtx_);
            pending_best_move_.reset();
        }
        thinking_.store(false);
    }

    void RequestBestMove(const std::vector<std::string>& moves_uci) {
        thinking_.store(true);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            pending_best_move_.reset();
        }
        SendLine(CmdPositionStartpos(moves_uci));
        if (config_.go_mode == GoMode::Depth)
            SendLine(CmdGoDepth(config_.go_value));
        else
            SendLine(CmdGoMovetime(config_.go_value));  // also covers Elo-limited mode (default 1000ms)
    }

    std::optional<std::string> TakeBestMove() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!pending_best_move_)
            return std::nullopt;
        std::string mv = *pending_best_move_;
        pending_best_move_.reset();
        return mv;
    }

    bool IsThinking() const { return thinking_.load(); }

private:
    bool WaitFor(bool& flag, std::chrono::seconds timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        return cv_.wait_for(lock, timeout, [&] { return flag; });
    }

    void SendLine(const std::string& line) {
        if (!to_engine_)
            return;
        std::fputs(line.c_str(), to_engine_);
        std::fputc('\n', to_engine_);
        std::fflush(to_engine_);
    }

    void ReaderLoop() {
        char* buf = nullptr;
        size_t cap = 0;
        while (reader_running_.load()) {
            ssize_t n = ::getline(&buf, &cap, from_engine_);
            if (n < 0)
                break;  // EOF / engine gone
            std::string line(buf, (n > 0 && buf[n - 1] == '\n') ? n - 1 : n);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (IsUciOk(line)) {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    uciok_received_ = true;
                }
                cv_.notify_all();
            } else if (IsReadyOk(line)) {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    readyok_received_ = true;
                }
                cv_.notify_all();
            } else if (auto bm = ParseBestMove(line)) {
                {
                    std::lock_guard<std::mutex> lock(mtx_);
                    pending_best_move_ = bm->move;
                }
                thinking_.store(false);
                if (on_update_)
                    on_update_();
            }
            // else: silently ignore (startup banner, id, option, info, ...)
        }
        free(buf);
    }

    std::string engine_path_;
    std::function<void()> on_update_;
    DifficultyConfig config_;

    pid_t pid_ = -1;
    FILE* to_engine_ = nullptr;
    FILE* from_engine_ = nullptr;
    std::thread reader_thread_;
    std::atomic<bool> reader_running_{false};
    std::atomic<bool> thinking_{false};

    // The only state shared across threads: {uciok_received_, readyok_received_,
    // pending_best_move_} guarded by mtx_, plus the atomics above. to_engine_ is
    // written only from the UI thread; from_engine_ is read only from the
    // reader thread -- no shared mutable state between them.
    std::mutex mtx_;
    std::condition_variable cv_;
    bool uciok_received_ = false;
    bool readyok_received_ = false;
    std::optional<std::string> pending_best_move_;
};
