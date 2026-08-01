#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <chrono>
#include <random>

#include <camera.h>
#include <common/animation_timer.h>
#include <common/bouncing_scene.h>
#include <mesh.h>
#include <pipeline.h>
#include <transform.h>

using namespace ftxui;

namespace {

constexpr double kSpeed = 0.6;  // canvas dots per tick
constexpr auto kTickInterval = std::chrono::milliseconds(33);

struct CanvasSize {
  int w, h;
};

// Overhead: title(1) + separator(1) + border top/bottom(2) = 4, matching
// rotating_triangle's accounting; single source of truth so the renderer
// and the bounce physics always agree on the play field's size.
CanvasSize compute_canvas_size(Dimensions term) {
  int avail_rows = std::max(4, term.dimy - 4);
  int avail_cols = std::max(10, term.dimx - 2);
  return {avail_cols * 2, avail_rows * 4};
}

}  // namespace

int main() {
  auto screen = ScreenInteractive::Fullscreen();

  common::Angles angles;

  std::mt19937 rng{std::random_device{}()};
  CanvasSize initial = compute_canvas_size(Terminal::Size());
  common::Scene scene =
      common::init_scene(rng, initial.w / 2.0, initial.h / 2.0, kSpeed);

  auto renderer = Renderer([&] {
    CanvasSize size = compute_canvas_size(Terminal::Size());
    auto draw_fn = [&](Canvas& c) { render_frame(angles, scene, c); };
    auto canvas_elem = canvas(size.w, size.h, draw_fn) | border | flex;

    return vbox({
        text("Tetrahedron — press q to quit") | bold | center,
        separator(),
        canvas_elem,
    });
  });

  auto app = CatchEvent(renderer, [&](Event e) -> bool {
    if (e == Event::Custom) {
      advance(angles);

      CanvasSize size = compute_canvas_size(Terminal::Size());
      double scale = nominal_scale(size.w, size.h);
      common::update_scene(scene, size.w, size.h, scale * kCircumradius);

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
