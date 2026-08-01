#pragma once

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/box.hpp>
#include <ftxui/screen/color.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace common {

// Shared layout helpers for the "NxM grid of colored terminal cells sized by
// a scale factor" rendering shape used by grid-based demos (sokoban, chess).
// `using namespace ftxui` is kept function-local (never at file scope) so
// including this header can't shadow an app's own identically-named types --
// exactly the class of bug that bit the chess app's own Color enum earlier.

// Renders a single scale-tall, 2*scale-wide cell: `glyph` (if non-empty)
// centered on the middle row, blank rows elsewhere, colored bg/fg.
inline ftxui::Element RenderGlyphCell(ftxui::Color bg, ftxui::Color fg, const std::string& glyph, int scale) {
    using namespace ftxui;
    int width = 2 * scale;
    int mid_row = scale / 2;
    Elements rows;
    for (int y = 0; y < scale; ++y) {
        if (y == mid_row && !glyph.empty()) {
            int left = (width - 1) / 2;
            int right = width - 1 - left;
            rows.push_back(text(std::string(left, ' ') + glyph + std::string(right, ' ')));
        } else {
            rows.push_back(text(std::string(width, ' ')));
        }
    }
    return vbox(std::move(rows)) | bgcolor(bg) | color(fg) | bold;
}

// Assembles a rows x cols grid of cells, each produced by cell_fn(r, c).
inline ftxui::Element RenderGrid(int rows, int cols, const std::function<ftxui::Element(int, int)>& cell_fn) {
    using namespace ftxui;
    Elements grid_rows;
    for (int r = 0; r < rows; ++r) {
        Elements grid_cols;
        for (int c = 0; c < cols; ++c)
            grid_cols.push_back(cell_fn(r, c));
        grid_rows.push_back(hbox(std::move(grid_cols)));
    }
    return vbox(std::move(grid_rows));
}

// Largest cell scale (rows tall, 2*scale wide) that fits `rows` x `cols`
// cells within the given available terminal rows/cols.
inline int ComputeGridScale(int avail_rows, int avail_cols, int rows, int cols) {
    int scale_r = std::max(1, avail_rows / rows);
    int scale_c = std::max(1, avail_cols / (cols * 2));
    return std::max(1, std::min(scale_r, scale_c));
}

// Inverse of the RenderGrid/RenderGlyphCell layout: maps a terminal-cell
// mouse position (mx, my) into the (row, col) grid cell it falls within,
// given the on-screen Box the grid was rendered into and the same `scale`
// used to render it. Kept beside ComputeGridScale so the two can never drift
// apart if the cell-size formula ever changes.
inline std::pair<int, int> PixelToCell(int mx, int my, const ftxui::Box& box, int scale) {
    int row = (my - box.y_min) / scale;
    int col = (mx - box.x_min) / (2 * scale);
    return {row, col};
}

}  // namespace common
