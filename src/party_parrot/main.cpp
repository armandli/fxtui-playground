#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "common/animation_timer.h"
#include "parrot_frames.h"

using namespace common;
using namespace ftxui;

namespace {

// Looks up the palette color for source pixel (sx, sy) in the given frame,
// or Color::Black if that pixel is transparent in the source GIF.
Color SourcePixelColor(int frame, int sx, int sy) {
    uint8_t idx = kFrameIndices[frame][sy * kFrameWidth + sx];
    if (idx == kTransparentIndex) return Color::Black;
    const auto& rgb = kPalette[idx];
    return Color::RGB(rgb[0], rgb[1], rgb[2]);
}

}  // namespace

int main() {
    auto screen = ScreenInteractive::Fullscreen();

    int frame_index = 0;

    // Renders the current frame full-bleed: two source pixel-rows per
    // terminal row via the upper-half-block glyph (independent fg/bg
    // colors), nearest-neighbor scaled from the fixed 128x128 source into
    // whatever the live terminal size is, letterboxed to preserve the
    // source's 1:1 aspect ratio (terminal cells are roughly twice as tall
    // as wide, so an N-column x N-row block of half-block cells renders as
    // an approximately square NxN pixel block).
    auto renderer = Renderer([&] {
        auto term = Terminal::Size();
        int cols = std::max(1, term.dimx);
        int term_rows = std::max(1, term.dimy);
        int pixel_rows = term_rows * 2;

        int size = std::min(cols, pixel_rows);
        int off_x = (cols - size) / 2;
        int off_y = (pixel_rows - size) / 2;

        Elements rows;
        rows.reserve(term_rows);
        for (int ty = 0; ty < term_rows; ++ty) {
            int py_top = ty * 2;
            int py_bot = ty * 2 + 1;
            bool row_in_top = py_top >= off_y && py_top < off_y + size;
            bool row_in_bot = py_bot >= off_y && py_bot < off_y + size;

            Elements cells;
            cells.reserve(cols);
            for (int tx = 0; tx < cols; ++tx) {
                Color fg = Color::Black;
                Color bg = Color::Black;

                if (tx >= off_x && tx < off_x + size) {
                    int sx = std::clamp((tx - off_x) * kFrameWidth / size, 0, kFrameWidth - 1);
                    if (row_in_top) {
                        int sy = std::clamp((py_top - off_y) * kFrameHeight / size, 0, kFrameHeight - 1);
                        fg = SourcePixelColor(frame_index, sx, sy);
                    }
                    if (row_in_bot) {
                        int sy = std::clamp((py_bot - off_y) * kFrameHeight / size, 0, kFrameHeight - 1);
                        bg = SourcePixelColor(frame_index, sx, sy);
                    }
                }
                cells.push_back(text("\xE2\x96\x80") | color(fg) | bgcolor(bg));
            }
            rows.push_back(hbox(std::move(cells)));
        }
        return vbox(std::move(rows));
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Custom) {
            frame_index = (frame_index + 1) % kFrameCount;
            return false;  // let the renderer run after the state update
        }
        if (e == Event::Character('q') || e == Event::Character('Q')) {
            screen.Exit();
            return true;
        }
        return false;
    });

    AnimationTimer timer(screen, std::chrono::milliseconds(kFrameDelayMs));

    screen.Loop(app);
}
