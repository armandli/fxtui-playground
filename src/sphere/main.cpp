#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <chrono>
#include <random>
#include <string>
#include <vector>

#include <common/animation_timer.h>
#include <common/bouncing_scene.h>
#include <pipeline.h>
#include <sphere_field.h>
#include <transform.h>

using namespace ftxui;

namespace {

constexpr double kSpeed = 0.5;  // physics units per tick
constexpr auto kTickInterval = std::chrono::milliseconds(33);

struct GridSize {
  int cols, rows;
};

// Overhead: title(1) + separator(1) + border top/bottom(2) + border sides(2).
GridSize compute_grid_size(Dimensions term) {
  int cols = std::max(10, term.dimx - 2);
  int rows = std::max(4, term.dimy - 4);
  return {cols, rows};
}

}  // namespace

int main() {
  auto screen = ScreenInteractive::Fullscreen();

  std::mt19937 rng{std::random_device{}()};
  common::Angles angles;
  RotationRates rates = init_rotation_rates(rng);
  LightState light;

  GridSize initial = compute_grid_size(Terminal::Size());
  common::Scene scene =
      common::init_scene(rng, initial.cols / 2.0, initial.rows, kSpeed);

  auto renderer = Renderer([&] {
    GridSize size = compute_grid_size(Terminal::Size());
    std::vector<char> chars =
        render_frame(angles, scene, light, size.cols, size.rows);

    Elements rows;
    rows.reserve(size.rows);
    for (int r = 0; r < size.rows; ++r) {
      Elements cells;
      cells.reserve(size.cols);
      for (int c = 0; c < size.cols; ++c) {
        cells.push_back(text(std::string(1, chars[r * size.cols + c])));
      }
      rows.push_back(hbox(std::move(cells)));
    }
    auto grid_elem = vbox(std::move(rows)) | border | flex;

    return vbox({
        text("Sphere — press q to quit") | bold | center,
        separator(),
        grid_elem,
    });
  });

  auto app = CatchEvent(renderer, [&](Event e) -> bool {
    if (e == Event::Custom) {
      advance(angles, rates);
      advance_light(light, rng);

      GridSize size = compute_grid_size(Terminal::Size());
      double radius = compute_radius(size.cols, size.rows);
      common::update_scene(scene, size.cols, size.rows * 2, radius);

      return false;  // let the renderer run after the state update
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
