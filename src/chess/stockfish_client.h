#ifndef STOCKFISH_CLIENT_H
#define STOCKFISH_CLIENT_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <common/child_process.h>
#include <uci_protocol.h>

// Thin UCI-specific wrapper around common::ChildProcess: it owns the
// mutex/condvar-guarded uciok/readyok/bestmove handshake state (chess
// move-flow specific), while the generic spawn/pipe/reader-thread/shutdown
// lifecycle lives in ChildProcess.
struct StockfishClient {
  enum class GoMode : int { Movetime, Depth };

  struct DifficultyConfig {
    bool limit_strength = true;
    int elo = 1500;
    GoMode go_mode = GoMode::Movetime;
    int go_value = 1000;
  };

  StockfishClient(std::string engine_path, std::function<void()> on_update)
      : on_update_(std::move(on_update)),
        process_(
            std::move(engine_path),
            [this](std::string_view line) { handle_line(line); }) {}

  StockfishClient(const StockfishClient&) = delete;
  StockfishClient& operator=(const StockfishClient&) = delete;

  ~StockfishClient() {
    process_.write_line(cmd_quit());
    // process_'s own destructor (escalate-then-join shutdown) runs next.
  }

  bool start(const DifficultyConfig& config) {
    config_ = config;
    if (not process_.start())
      return false;

    process_.write_line(cmd_uci());
    if (not wait_for(uciok_received_, std::chrono::seconds(5)))
      return false;

    process_.write_line(cmd_set_option(
        "UCI_LimitStrength", config_.limit_strength ? "true" : "false"));
    if (config_.limit_strength)
      process_.write_line(
          cmd_set_option("UCI_Elo", std::to_string(config_.elo)));

    process_.write_line(cmd_is_ready());
    if (not wait_for(readyok_received_, std::chrono::seconds(5)))
      return false;
    return true;
  }

  void new_game() {
    process_.write_line(cmd_uci_new_game());
    // fire-and-forget; commands are processed strictly in order
    process_.write_line(cmd_is_ready());
  }

  // Cancels an in-flight search and discards any stale pending result.
  // Call before new_game() whenever resetting mid-search, otherwise a
  // search already in flight when "ucinewgame" is sent violates the UCI
  // protocol and its eventual bestmove could land on the new game.
  void cancel_pending() {
    process_.write_line(cmd_stop());
    {
      std::lock_guard<std::mutex> lock(mtx_);
      pending_best_move_.reset();
    }
    thinking_.store(false);
  }

  void request_best_move(const std::vector<std::string>& moves_uci) {
    thinking_.store(true);
    {
      std::lock_guard<std::mutex> lock(mtx_);
      pending_best_move_.reset();
    }
    process_.write_line(cmd_position_startpos(moves_uci));
    if (config_.go_mode == GoMode::Depth)
      process_.write_line(cmd_go_depth(config_.go_value));
    else
      // also covers Elo-limited mode (default 1000ms)
      process_.write_line(cmd_go_movetime(config_.go_value));
  }

  std::optional<std::string> take_best_move() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (not pending_best_move_)
      return std::nullopt;
    std::string mv = *pending_best_move_;
    pending_best_move_.reset();
    return mv;
  }

  bool is_thinking() const { return thinking_.load(); }

protected:
  bool wait_for(bool& flag, std::chrono::seconds timeout) {
    std::unique_lock<std::mutex> lock(mtx_);
    return cv_.wait_for(lock, timeout, [&] { return flag; });
  }

  // Invoked from ChildProcess's reader thread for every line the engine
  // prints. Everything that isn't uciok/readyok/bestmove (the startup
  // banner, id, option, info, ...) simply matches none of these and falls
  // through, silently ignored.
  void handle_line(std::string_view line) {
    if (is_uci_ok(line)) {
      {
        std::lock_guard<std::mutex> lock(mtx_);
        uciok_received_ = true;
      }
      cv_.notify_all();
    } else if (is_ready_ok(line)) {
      {
        std::lock_guard<std::mutex> lock(mtx_);
        readyok_received_ = true;
      }
      cv_.notify_all();
    } else if (auto bm = parse_best_move(line)) {
      {
        std::lock_guard<std::mutex> lock(mtx_);
        pending_best_move_ = bm->move;
      }
      thinking_.store(false);
      if (on_update_)
        on_update_();
    }
  }

private:
  std::function<void()> on_update_;
  DifficultyConfig config_;
  common::ChildProcess process_;
  std::atomic<bool> thinking_{false};

  // The only state shared across threads: {uciok_received_, readyok_received_,
  // pending_best_move_} guarded by mtx_. process_'s own write_line is called
  // only from the UI thread; handle_line is called only from the reader
  // thread -- no other shared mutable state between them.
  std::mutex mtx_;
  std::condition_variable cv_;
  bool uciok_received_ = false;
  bool readyok_received_ = false;
  std::optional<std::string> pending_best_move_;
};

#endif  // STOCKFISH_CLIENT_H
