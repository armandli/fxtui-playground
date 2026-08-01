#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "common/child_process.h"
#include "uci_protocol.h"

using namespace common;

// Thin UCI-specific wrapper around common::ChildProcess: it owns the
// mutex/condvar-guarded uciok/readyok/bestmove handshake state (chess
// move-flow specific), while the generic spawn/pipe/reader-thread/shutdown
// lifecycle lives in ChildProcess.
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
        : on_update_(std::move(on_update)),
          process_(std::move(engine_path), [this](const std::string& line) { HandleLine(line); }) {}

    StockfishClient(const StockfishClient&) = delete;
    StockfishClient& operator=(const StockfishClient&) = delete;

    ~StockfishClient() {
        process_.WriteLine(CmdQuit());
        // process_'s own destructor (escalate-then-join shutdown) runs next.
    }

    bool Start(const DifficultyConfig& config) {
        config_ = config;
        if (!process_.Start())
            return false;

        process_.WriteLine(CmdUci());
        if (!WaitFor(uciok_received_, std::chrono::seconds(5)))
            return false;

        process_.WriteLine(CmdSetOption("UCI_LimitStrength", config_.limit_strength ? "true" : "false"));
        if (config_.limit_strength)
            process_.WriteLine(CmdSetOption("UCI_Elo", std::to_string(config_.elo)));

        process_.WriteLine(CmdIsReady());
        if (!WaitFor(readyok_received_, std::chrono::seconds(5)))
            return false;
        return true;
    }

    void NewGame() {
        process_.WriteLine(CmdUciNewGame());
        process_.WriteLine(CmdIsReady());  // fire-and-forget; commands are processed strictly in order
    }

    // Cancels an in-flight search and discards any stale pending result.
    // Call before NewGame() whenever resetting mid-search, otherwise a
    // search already in flight when "ucinewgame" is sent violates the UCI
    // protocol and its eventual bestmove could land on the new game.
    void CancelPending() {
        process_.WriteLine(CmdStop());
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
        process_.WriteLine(CmdPositionStartpos(moves_uci));
        if (config_.go_mode == GoMode::Depth)
            process_.WriteLine(CmdGoDepth(config_.go_value));
        else
            process_.WriteLine(CmdGoMovetime(config_.go_value));  // also covers Elo-limited mode (default 1000ms)
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

    // Invoked from ChildProcess's reader thread for every line the engine
    // prints. Everything that isn't uciok/readyok/bestmove (the startup
    // banner, id, option, info, ...) simply matches none of these and falls
    // through, silently ignored.
    void HandleLine(const std::string& line) {
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
    }

    std::function<void()> on_update_;
    DifficultyConfig config_;
    ChildProcess process_;
    std::atomic<bool> thinking_{false};

    // The only state shared across threads: {uciok_received_, readyok_received_,
    // pending_best_move_} guarded by mtx_. process_'s own WriteLine is called
    // only from the UI thread; HandleLine is called only from the reader
    // thread -- no other shared mutable state between them.
    std::mutex mtx_;
    std::condition_variable cv_;
    bool uciok_received_ = false;
    bool readyok_received_ = false;
    std::optional<std::string> pending_best_move_;
};
