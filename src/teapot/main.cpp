#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/terminal.hpp>

#include <chrono>
#include <random>
#include <string>
#include <vector>

#include <camera.h>
#include <common/animation_timer.h>
#include <common/bouncing_scene.h>
#include <common/grid_size.h>
#include <common/light_state.h>
#include <common/rotation_state.h>
#include <pipeline.h>

using namespace ftxui;

namespace {

// Slower than sphere's 0.5 -- the teapot's on-screen silhouette is bigger,
// so the same speed would read as brisker drift across the frame.
constexpr double kSpeed = 0.4;
constexpr auto kTickInterval = std::chrono::milliseconds(33);

}  // namespace

int main() {
  auto screen = ScreenInteractive::Fullscreen();

  std::mt19937 rng{std::random_device{}()};
  common::Angles angles;
  common::RotationRates rates = common::init_rotation_rates(rng);
  common::LightState light;

  common::GridSize initial = common::compute_grid_size(Terminal::Size());
  common::Scene scene =
      common::init_scene(rng, initial.cols / 2.0, initial.rows, kSpeed);

  auto renderer = Renderer([&] {
    common::GridSize size = common::compute_grid_size(Terminal::Size());
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
        text("Teapot — press q to quit") | bold | center,
        separator(),
        grid_elem,
    });
  });

  auto app = CatchEvent(renderer, [&](Event e) -> bool {
    if (e == Event::Custom) {
      common::advance_angles(angles, rates);
      common::advance_light(light, rng);

      common::GridSize size = common::compute_grid_size(Terminal::Size());
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
