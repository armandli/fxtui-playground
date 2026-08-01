#ifndef CHILD_PROCESS_H
#define CHILD_PROCESS_H

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>

// Generic child-process lifecycle: spawn a line-oriented subprocess, read its
// stdout on a background thread, write commands to its stdin, and tear it
// down cleanly (escalate-then-join) on destruction. This is the reusable
// infrastructure underneath chess's UCI/Stockfish communication -- the UCI
// vocabulary itself stays in src/chess/, since it's inherently chess-specific
// and isn't duplicated anywhere else in this repo.

namespace common {

// Tracks the pid of the most recently spawned ChildProcess so a top-level
// fatal-signal handler can forward SIGTERM to it before re-raising. Only one
// ChildProcess is expected to be live at a time per process.
inline std::atomic<pid_t> g_child_pid{-1};

// Best-effort: SIGTERM + WNOHANG-reap g_child_pid (if any), then restore the
// signal's default disposition and re-raise it. Install via
// signal(SIGINT, common::terminate_child_and_reraise) (and SIGTERM) at the
// top of main(), before constructing a ChildProcess -- this only matters in
// the window before an FTXUI ScreenInteractive::Loop() has installed its own
// signal handling, since that takes over gracefully once running.
inline void terminate_child_and_reraise(int sig) {
  pid_t pid = g_child_pid.load(std::memory_order_relaxed);
  if (pid > 0) {
    kill(pid, SIGTERM);
    waitpid(pid, nullptr, WNOHANG);
  }
  signal(sig, SIG_DFL);
  raise(sig);
}

namespace detail {

// Forks and execs `path` with `args`, stdin/stdout redirected to pipes
// (`in_write_fd`/`out_read_fd` are the parent's ends), stderr to /dev/null.
// Returns the child pid, or -1 on failure. The child branch never
// returns/throws/exit()s -- only _exit() or a successful exec() may end it,
// to avoid double-flushing iostream buffer state inherited from the parent.
inline pid_t spawn_child_process(
    const std::string& path,
    const std::vector<std::string>& args,
    int& in_write_fd,
    int& out_read_fd) {
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
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(path.c_str()));
    for (const auto& a : args)
      argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    execvp(path.c_str(), argv.data());
    _exit(127);  // exec failed
  }

  // Parent
  close(in_pipe[0]);
  close(out_pipe[1]);
  in_write_fd = in_pipe[1];
  out_read_fd = out_pipe[0];
  g_child_pid.store(pid, std::memory_order_relaxed);
  return pid;
}

}  // namespace detail

struct ChildProcess {
  ChildProcess(
      std::string path,
      std::vector<std::string> args,
      std::function<void(const std::string&)> on_line)
      : path_(std::move(path)),
        args_(std::move(args)),
        on_line_(std::move(on_line)) {}

  ChildProcess(
      std::string path,
      std::function<void(const std::string&)> on_line)
      : ChildProcess(std::move(path), {}, std::move(on_line)) {}

  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;

  ~ChildProcess() {
    if (to_child_) {
      std::fclose(to_child_);
      to_child_ = nullptr;
    }
    if (pid_ > 0) {
      auto deadline = std::chrono::steady_clock::now()
          + std::chrono::seconds(2);
      int status = 0;
      pid_t r = 0;
      while (std::chrono::steady_clock::now() < deadline) {
        r = waitpid(pid_, &status, WNOHANG);
        if (r == pid_ or r == -1)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      if (r != pid_) {
        kill(pid_, SIGKILL);
        waitpid(pid_, &status, 0);
      }
      g_child_pid.store(-1, std::memory_order_relaxed);
      pid_ = -1;
    }
    reader_running_.store(false);
    if (reader_thread_.joinable())
      reader_thread_.join();  // fast: the process is confirmed dead above
    if (from_child_) {
      std::fclose(from_child_);
      from_child_ = nullptr;
    }
  }

  // Spawns the process, opens both ends, and starts the background reader
  // thread. Returns false on any failure (pipe/fork/fdopen).
  bool start() {
    int in_fd = -1, out_fd = -1;
    pid_ = detail::spawn_child_process(path_, args_, in_fd, out_fd);
    if (pid_ < 0)
      return false;
    to_child_ = fdopen(in_fd, "w");
    from_child_ = fdopen(out_fd, "r");
    if (not to_child_ or not from_child_)
      return false;

    reader_running_.store(true);
    reader_thread_ = std::thread(&ChildProcess::reader_loop, this);
    return true;
  }

  void write_line(const std::string& line) {
    if (not to_child_)
      return;
    std::fputs(line.c_str(), to_child_);
    std::fputc('\n', to_child_);
    std::fflush(to_child_);
  }

protected:
  void reader_loop() {
    char* buf = nullptr;
    size_t cap = 0;
    while (reader_running_.load()) {
      ssize_t n = ::getline(&buf, &cap, from_child_);
      if (n < 0)
        break;  // EOF / process gone
      std::string line(buf, (n > 0 and buf[n - 1] == '\n') ? n - 1 : n);
      if (not line.empty() and line.back() == '\r')
        line.pop_back();
      if (on_line_)
        on_line_(line);
    }
    free(buf);
  }

private:
  std::string path_;
  std::vector<std::string> args_;
  std::function<void(const std::string&)> on_line_;

  pid_t pid_ = -1;
  FILE* to_child_ = nullptr;
  FILE* from_child_ = nullptr;
  std::thread reader_thread_;
  std::atomic<bool> reader_running_{false};
};

}  // namespace common

#endif  // CHILD_PROCESS_H
