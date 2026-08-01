#ifndef ANIMATION_TIMER_H
#define ANIMATION_TIMER_H

#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>

#include <atomic>
#include <chrono>
#include <thread>

namespace common {

// RAII replacement for the "background thread posts Event::Custom on an
// interval" idiom used by every plain-animation demo in this repo. Construct
// right before screen.Loop(app); the thread starts immediately and the
// destructor (running once screen.Loop() returns and this object goes out of
// scope) stops and joins it.
struct AnimationTimer {
  AnimationTimer(
      ftxui::ScreenInteractive& screen,
      std::chrono::milliseconds interval)
      : screen_(screen), interval_(interval), thread_([this] { run(); }) {}

  ~AnimationTimer() {
    running_.store(false, std::memory_order_relaxed);
    thread_.join();
  }

  AnimationTimer(const AnimationTimer&) = delete;
  AnimationTimer& operator=(const AnimationTimer&) = delete;

protected:
  void run() {
    while (running_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(interval_);
      if (running_.load(std::memory_order_relaxed))
        screen_.PostEvent(ftxui::Event::Custom);
    }
  }

private:
  ftxui::ScreenInteractive& screen_;
  std::chrono::milliseconds interval_;
  std::atomic<bool> running_{true};
  std::thread thread_;
};

}  // namespace common

#endif  // ANIMATION_TIMER_H
