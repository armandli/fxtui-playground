#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <random>
#include <string>
#include <vector>

using namespace ftxui;

namespace {

constexpr int    kNumBins    = 101;
constexpr int    kNumSamples = 100'000;
constexpr double kRangeMin   = -4.0;
constexpr double kRangeMax   =  4.0;
constexpr int    kCenterBar  = 50;
constexpr int    kYAxisWidth = 8;  // terminal columns reserved for Y-axis labels

Element build_x_axis(int canvas_cols, int y_axis_width) {
    const std::vector<std::pair<double, std::string>> ticks = {
        {-4.0, "-4"}, {-2.0, "-2"}, {0.0, "0"}, {2.0, "2"}, {4.0, "4"}
    };
    std::string line(canvas_cols, ' ');
    for (auto& [v, label] : ticks) {
        int pos = static_cast<int>((v - kRangeMin) / (kRangeMax - kRangeMin) * canvas_cols);
        pos = std::clamp(pos, 0, canvas_cols - static_cast<int>(label.size()));
        for (int j = 0; j < static_cast<int>(label.size()); ++j)
            line[pos + j] = label[j];
    }
    return hbox({
        text(std::string(y_axis_width + 1, ' ')),  // +1 for left border char
        text(line),
    });
}

}  // namespace

int main() {
    // ── Sample 100,000 values from N(0,1) and bin them ───────────────────────
    std::array<int, kNumBins> counts{};
    {
        std::mt19937_64 rng(std::random_device{}());
        std::normal_distribution<double> dist(0.0, 1.0);
        constexpr double bin_w = (kRangeMax - kRangeMin) / kNumBins;
        for (int i = 0; i < kNumSamples; ++i) {
            double v = dist(rng);
            int bin = static_cast<int>((v - kRangeMin) / bin_w);
            if (bin >= 0 && bin < kNumBins)
                ++counts[bin];
        }
    }
    int max_count = *std::max_element(counts.begin(), counts.end());

    // ── Interactive state ─────────────────────────────────────────────────────
    int highlighted   = kCenterBar;
    Box histogram_box;                 // filled by reflect() each frame

    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer([&] {
        auto term = Terminal::Size();

        // Layout overhead: title(1) + sep(1) + border_top(1) + border_bot(1)
        //                  + x_axis(1) + sep(1) + status(1) = 7 rows
        int avail_rows = std::max(4, term.dimy - 7);
        // Overhead: y_axis(kYAxisWidth) + border_left(1) + border_right(1) = kYAxisWidth + 2
        int avail_cols = std::max(10, term.dimx - kYAxisWidth - 2);

        // Canvas in braille units: 2 per terminal col, 4 per terminal row
        int canvas_w = avail_cols * 2;
        int canvas_h = avail_rows * 4;

        // Capture canvas dimensions by value — canvas fn is called after Renderer returns
        auto draw_fn = [canvas_w, canvas_h, &counts, max_count, &highlighted](Canvas& c) {
            double bin_w = static_cast<double>(canvas_w) / kNumBins;
            for (int i = 0; i < kNumBins; ++i) {
                Color col = (i == highlighted) ? Color::Blue : Color::Green;
                int x0 = static_cast<int>(i * bin_w);
                int x1 = static_cast<int>((i + 1) * bin_w) - 1;
                if (x1 < x0) x1 = x0;
                int bar_h = max_count > 0
                    ? static_cast<int>(static_cast<double>(counts[i]) / max_count * canvas_h)
                    : 0;
                for (int y = canvas_h - bar_h; y < canvas_h; ++y)
                    c.DrawBlockLine(x0, y, x1, y, col);
            }
        };

        // Y-axis: 1-row spacers at top/bottom align labels with canvas content inside border
        auto y_axis = vbox({
            filler() | size(HEIGHT, EQUAL, 1),
            text(std::format("{:>{}}", max_count, kYAxisWidth - 1)) | align_right,
            filler(),
            text(std::format("{:>{}}", 0, kYAxisWidth - 1)) | align_right,
            filler() | size(HEIGHT, EQUAL, 1),
        }) | size(WIDTH, EQUAL, kYAxisWidth);

        // Canvas element: reflect captures content box before border is added
        auto canvas_elem = canvas(canvas_w, canvas_h, draw_fn)
                           | reflect(histogram_box)
                           | border
                           | flex;

        // Status bar
        constexpr double val_per_bin = (kRangeMax - kRangeMin) / kNumBins;
        double lo = kRangeMin + highlighted * val_per_bin;
        double hi = lo + val_per_bin;
        auto status = text(std::format(
            "Bar {:3d} | [{:+.3f}, {:+.3f}] | Count: {:6d} | arrow keys / click to navigate | q to quit",
            highlighted, lo, hi, counts[highlighted]
        )) | center;

        return vbox({
            text("Normal Distribution Histogram  (N=100,000 samples, 101 bins)")
                | bold | center,
            separator(),
            hbox({ y_axis, canvas_elem }) | flex,
            build_x_axis(avail_cols, kYAxisWidth),
            separator(),
            status,
        });
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::ArrowLeft) {
            if (highlighted > 0) --highlighted;
            return true;
        }
        if (e == Event::ArrowRight) {
            if (highlighted < kNumBins - 1) ++highlighted;
            return true;
        }
        if (e.is_mouse()
            && e.mouse().button == Mouse::Left
            && e.mouse().motion == Mouse::Pressed) {
            int mx = e.mouse().x;
            int my = e.mouse().y;
            if (histogram_box.Contain(mx, my)) {
                int total = histogram_box.x_max - histogram_box.x_min + 1;
                int bar   = (mx - histogram_box.x_min) * kNumBins / total;
                highlighted = std::clamp(bar, 0, kNumBins - 1);
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
