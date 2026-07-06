#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <random>
#include <string>
#include <vector>

using namespace ftxui;

namespace {

constexpr int    kN            = 80;
constexpr int    kInfoWidth    = 22;
constexpr int    kSelectRadius = 6;   // braille units
constexpr double kSigma        = 0.15;
constexpr double kPadFraction  = 0.05;

const Color kColor0   = Color::Cyan;
const Color kColor1   = Color::Green;
const Color kSelected = Color::Yellow;

struct Point {
    double x, y;
    int    series;
};

std::vector<Point> generate_points() {
    std::mt19937 rng(42);
    std::normal_distribution<double> noise(0.0, kSigma);
    std::vector<Point> pts;
    pts.reserve(kN * 2);
    // Ellipse 0: x=3cos(t), y=1.5sin(t), center (0,0)
    for (int i = 0; i < kN; ++i) {
        double t = 2.0 * std::numbers::pi * i / kN;
        pts.push_back({3.0 * std::cos(t) + noise(rng),
                       1.5 * std::sin(t) + noise(rng), 0});
    }
    // Ellipse 1: x=1.5cos(t)+1, y=2.5sin(t)+2, center (1,2)
    for (int i = 0; i < kN; ++i) {
        double t = 2.0 * std::numbers::pi * i / kN;
        pts.push_back({1.5 * std::cos(t) + 1.0 + noise(rng),
                       2.5 * std::sin(t) + 2.0 + noise(rng), 1});
    }
    return pts;
}

// Maps data coordinates to braille canvas pixel coordinates.
std::pair<int, int> data_to_canvas(double x, double y,
                                   double xmin, double xmax,
                                   double ymin, double ymax,
                                   int canvas_w, int canvas_h) {
    int px = static_cast<int>((x - xmin) / (xmax - xmin) * (canvas_w - 1));
    int py = canvas_h - 1
           - static_cast<int>((y - ymin) / (ymax - ymin) * (canvas_h - 1));
    px = std::clamp(px, 0, canvas_w - 1);
    py = std::clamp(py, 0, canvas_h - 1);
    return {px, py};
}

}  // namespace

int main() {
    auto points = generate_points();

    // Compute data bounds once with padding.
    double raw_xmin = points[0].x, raw_xmax = points[0].x;
    double raw_ymin = points[0].y, raw_ymax = points[0].y;
    for (auto& p : points) {
        raw_xmin = std::min(raw_xmin, p.x); raw_xmax = std::max(raw_xmax, p.x);
        raw_ymin = std::min(raw_ymin, p.y); raw_ymax = std::max(raw_ymax, p.y);
    }
    const double pad_x = (raw_xmax - raw_xmin) * kPadFraction;
    const double pad_y = (raw_ymax - raw_ymin) * kPadFraction;
    const double xmin = raw_xmin - pad_x, xmax = raw_xmax + pad_x;
    const double ymin = raw_ymin - pad_y, ymax = raw_ymax + pad_y;

    int selected_idx = -1;
    Box canvas_box{};   // filled by reflect() each frame; zero-init → Contain returns false

    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer([&] {
        auto term = Terminal::Size();

        // Overhead: title(1) + sep(1) + border_top(1) + border_bot(1) + sep(1) + status(1) = 6
        int avail_rows = std::max(4, term.dimy - 6);
        // Overhead: border_left(1) + border_right(1) + info panel when selected
        int avail_cols = std::max(10, term.dimx - 2
                                      - (selected_idx >= 0 ? kInfoWidth : 0));

        int canvas_w = avail_cols * 2;
        int canvas_h = avail_rows * 4;

        auto draw_fn = [canvas_w, canvas_h, &points, &selected_idx,
                        xmin, xmax, ymin, ymax](Canvas& c) {
            // Draw all non-selected points first.
            for (int i = 0; i < static_cast<int>(points.size()); ++i) {
                if (i == selected_idx) continue;
                auto& p = points[i];
                auto [px, py] = data_to_canvas(p.x, p.y,
                                               xmin, xmax, ymin, ymax,
                                               canvas_w, canvas_h);
                Color col = (p.series == 0) ? kColor0 : kColor1;
                c.DrawPointCircle(px, py, 2, col);
            }
            // Draw selected point on top in yellow.
            if (selected_idx >= 0 && selected_idx < static_cast<int>(points.size())) {
                auto& sp = points[selected_idx];
                auto [px, py] = data_to_canvas(sp.x, sp.y,
                                               xmin, xmax, ymin, ymax,
                                               canvas_w, canvas_h);
                c.DrawPointCircle(px, py, 3, kSelected);
            }
        };

        auto canvas_elem = canvas(canvas_w, canvas_h, draw_fn)
                           | reflect(canvas_box)
                           | border
                           | flex;

        // Info panel — only present when a point is selected.
        Elements main_row = {canvas_elem};
        if (selected_idx >= 0) {
            auto& sp = points[selected_idx];
            auto info = window(
                text(" Selected Point "),
                vbox({
                    text(std::format("Series: {}", sp.series + 1)),
                    text(std::format("x: {:.2f}", sp.x)),
                    text(std::format("y: {:.2f}", sp.y)),
                })
            ) | size(WIDTH, EQUAL, kInfoWidth);
            main_row.push_back(info);
        }

        auto status = hbox({
            text("  "),
            text("●") | color(kColor0),
            text(" Ellipse 1  "),
            text("●") | color(kColor1),
            text(" Ellipse 2  "),
            text("●") | color(kSelected),
            text(" Selected  "),
            text("| Click to select point  q to quit"),
        }) | center;

        return vbox({
            text("Scatter Plot — Two Ellipses with Gaussian Noise  (N=80/series, σ=0.15)")
                | bold | center,
            separator(),
            hbox(main_row) | flex,
            separator(),
            status,
        });
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e.is_mouse()
            && e.mouse().button == Mouse::Left
            && e.mouse().motion == Mouse::Pressed) {
            int mx = e.mouse().x, my = e.mouse().y;
            if (canvas_box.Contain(mx, my)) {
                // Convert terminal cell click → braille canvas coordinates.
                int cx = (mx - canvas_box.x_min) * 2;
                int cy = (my - canvas_box.y_min) * 4;

                // Reconstruct canvas dimensions from box (matches last render).
                int cw = (canvas_box.x_max - canvas_box.x_min + 1) * 2;
                int ch = (canvas_box.y_max - canvas_box.y_min + 1) * 4;

                // Find nearest data point within selection radius.
                double best_dist = std::numeric_limits<double>::max();
                int    best_idx  = -1;
                for (int i = 0; i < static_cast<int>(points.size()); ++i) {
                    auto [px, py] = data_to_canvas(points[i].x, points[i].y,
                                                   xmin, xmax, ymin, ymax,
                                                   cw, ch);
                    double dx = px - cx, dy = py - cy;
                    double dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < best_dist) {
                        best_dist = dist;
                        best_idx  = i;
                    }
                }
                selected_idx = (best_dist < kSelectRadius) ? best_idx : -1;
                return true;
            }
        }
        if (e == Event::Character('q') || e == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    screen.Loop(app);
}
