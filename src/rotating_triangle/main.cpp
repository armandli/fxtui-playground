#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numbers>

#include "common/animation_timer.h"
#include "common/rasterizer2d.h"

using namespace common;
using namespace ftxui;

namespace {

// Rotates a point around (cx, cy) by `theta` radians. In this y-down canvas
// coordinate space, increasing theta with this formula turns the point
// clockwise as seen on screen (right -> down -> left -> up).
Point rotate(double cx, double cy, double radius, double base_angle, double theta) {
    double a = base_angle + theta;
    return {cx + radius * std::cos(a), cy + radius * std::sin(a)};
}

}  // namespace

int main() {
    double theta = 0.0;
    constexpr double kStep = 2.0 * std::numbers::pi / 180.0;  // 2 degrees per tick

    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer([&] {
        auto term = Terminal::Size();

        // Overhead: title(1) + separator(1) + border top/bottom(2) = 4
        int avail_rows = std::max(4, term.dimy - 4);
        int avail_cols = std::max(10, term.dimx - 2);

        int canvas_w = avail_cols * 2;
        int canvas_h = avail_rows * 4;

        auto draw_fn = [canvas_w, canvas_h, theta](Canvas& c) {
            double cx = canvas_w / 2.0;
            double cy = canvas_h / 2.0;
            double radius = 0.4 * std::min(canvas_w, canvas_h);

            Point v0 = rotate(cx, cy, radius, -std::numbers::pi / 2.0, theta);
            Point v1 = rotate(cx, cy, radius, std::numbers::pi / 6.0, theta);
            Point v2 = rotate(cx, cy, radius, 5.0 * std::numbers::pi / 6.0, theta);

            FillTriangle(c, v0, v1, v2, Color::Yellow);
        };

        auto canvas_elem = canvas(canvas_w, canvas_h, draw_fn) | border | flex;

        return vbox({
            text("Rotating Triangle — press q to quit") | bold | center,
            separator(),
            canvas_elem,
        });
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Custom) {
            theta += kStep;
            if (theta > 2.0 * std::numbers::pi) {
                theta -= 2.0 * std::numbers::pi;
            }
            return false;  // let the renderer run after state update
        }
        if (e == Event::Character('q') || e == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    AnimationTimer timer(screen, std::chrono::milliseconds(33));

    screen.Loop(app);
}
