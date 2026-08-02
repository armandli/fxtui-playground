#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <chrono>
#include <format>
#include <random>
#include <string>
#include <vector>

#include <common/angles.h>
#include <common/animation_timer.h>
#include <common/grid_size.h>
#include <pipeline.h>
#include <raindrop.h>

using namespace ftxui;

namespace {

constexpr auto kTickInterval = std::chrono::milliseconds(33);
constexpr double kAzimuthStep = 5.0 * common::kDegToRad;
constexpr double kSpawnRateStep = 0.5;
constexpr double kMaxSpawnRate = 8.0;
constexpr double kInitialSpawnRate = 2.0;

}  // namespace

int main() {
  auto screen = ScreenInteractive::Fullscreen();

  std::mt19937 rng{std::random_device{}()};
  std::vector<Drop> drops;
  double azimuth = 0.0;
  double spawn_rate = kInitialSpawnRate;

  auto renderer = Renderer([&] {
    common::GridSize size = common::compute_grid_size(Terminal::Size());
    RenderedFrame frame = render_frame(drops, azimuth, size.cols, size.rows);

    Elements rows;
    rows.reserve(size.rows);
    for (int r = 0; r < size.rows; ++r) {
      Elements cells;
      cells.reserve(size.cols);
      for (int c = 0; c < size.cols; ++c) {
        int i = r * size.cols + c;
        char ch = frame.chars[i];
        if (ch == ' ') {
          cells.push_back(text(" "));
        } else {
          cells.push_back(text(std::string(1, ch)) | color(frame.colors[i]));
        }
      }
      rows.push_back(hbox(std::move(cells)));
    }
    auto grid_elem = vbox(std::move(rows)) | border | flex;

    return vbox({
        text(std::format(
            "Rain — ↑/↓ rate: {:.1f}  ←/→ rotate — "
            "press q to quit",
            spawn_rate)) |
            bold | center,
        separator(),
        grid_elem,
    });
  });

  auto app = CatchEvent(renderer, [&](Event e) -> bool {
    if (e == Event::Custom) {
      spawn_new_drops(drops, spawn_rate, rng);
      advance_drops(drops);
      remove_grounded(drops);
      return false;  // let the renderer run after the state update
    }
    if (e == Event::ArrowUp) {
      spawn_rate =
          std::clamp(spawn_rate + kSpawnRateStep, 0.0, kMaxSpawnRate);
      return true;
    }
    if (e == Event::ArrowDown) {
      spawn_rate =
          std::clamp(spawn_rate - kSpawnRateStep, 0.0, kMaxSpawnRate);
      return true;
    }
    if (e == Event::ArrowLeft) {
      azimuth = common::wrap_angle(azimuth - kAzimuthStep);
      return true;
    }
    if (e == Event::ArrowRight) {
      azimuth = common::wrap_angle(azimuth + kAzimuthStep);
      return true;
    }
    if (e == Event::Character('q') or e == Event::Character('Q')) {
      screen.Exit();
      return true;
    }
    return false;
  });

  common::AnimationTimer timer(screen, kTickInterval);

  screen.Loop(app);
}
