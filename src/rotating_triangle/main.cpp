#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <numbers>
#include <thread>

using namespace ftxui;

namespace {

struct Point {
    double x, y;
};

// Rotates a point around (cx, cy) by `theta` radians. In this y-down canvas
// coordinate space, increasing theta with this formula turns the point
// clockwise as seen on screen (right -> down -> left -> up).
Point rotate(double cx, double cy, double radius, double base_angle, double theta) {
    double a = base_angle + theta;
    return {cx + radius * std::cos(a), cy + radius * std::sin(a)};
}

// Sign of the cross product (p2-p1) x (p-p1); used for the point-in-triangle test.
double edge_sign(const Point& p1, const Point& p2, double px, double py) {
    return (px - p2.x) * (p1.y - p2.y) - (p1.x - p2.x) * (py - p2.y);
}

bool point_in_triangle(double px, double py, const Point& a, const Point& b, const Point& c) {
    double d1 = edge_sign(a, b, px, py);
    double d2 = edge_sign(b, c, px, py);
    double d3 = edge_sign(c, a, px, py);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
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

            int min_x = static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x})));
            int max_x = static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x})));
            int min_y = static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y})));
            int max_y = static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y})));
            min_x = std::clamp(min_x, 0, canvas_w - 1);
            max_x = std::clamp(max_x, 0, canvas_w - 1);
            min_y = std::clamp(min_y, 0, canvas_h - 1);
            max_y = std::clamp(max_y, 0, canvas_h - 1);

            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    if (point_in_triangle(x, y, v0, v1, v2)) {
                        c.DrawPoint(x, y, true, Color::Yellow);
                    }
                }
            }
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

    // Timer thread: fires every ~33 ms (~30 FPS) and posts a Custom event to the main loop.
    std::atomic<bool> running{true};
    std::thread timer([&] {
        while (running.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            if (running.load(std::memory_order_relaxed))
                screen.PostEvent(Event::Custom);
        }
    });

    screen.Loop(app);

    running.store(false, std::memory_order_relaxed);
    timer.join();
}
