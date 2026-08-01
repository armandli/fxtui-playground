#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <iostream>
#include <string>

#include "level.h"

using namespace ftxui;

namespace {

// Each game cell renders as `scale` terminal rows tall and `2*scale` columns
// wide (double width so cells read roughly square despite typical terminal
// font proportions).
int ComputeScale(const Level& level, Dimensions term) {
    // Overhead outside the board: title(1) + sep(1) + sep(1) + status(1)
    // + board border(2 rows, 2 cols) = 6 rows, 2 cols.
    int avail_rows = std::max(1, term.dimy - 6);
    int avail_cols = std::max(1, term.dimx - 2);
    int scale_r = std::max(1, avail_rows / level.rows);
    int scale_c = std::max(1, avail_cols / (level.cols * 2));
    return std::min(scale_r, scale_c);
}

Element RenderCell(const Level& level, int r, int c, int scale) {
    Position pos{r, c};
    Cell base = At(level, pos);
    bool is_player = level.player == pos;
    bool has_box = HasBoxAt(level, pos);

    Color bg = Color::Black;
    Color fg = Color::White;
    std::string glyph;

    if (base == Cell::Wall) {
        bg = Color::Red;
    } else if (is_player) {
        bg = (base == Cell::Goal) ? Color::RGB(0, 90, 90) : Color::Black;
        fg = Color::White;
        glyph = "@";
    } else if (has_box) {
        bg = (base == Cell::Goal) ? Color::Green : Color::YellowLight;
    } else if (base == Cell::Goal) {
        fg = Color::CyanLight;
        glyph = "o";
    }

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

Element RenderBoard(const Level& level, int scale) {
    Elements rows;
    for (int r = 0; r < level.rows; ++r) {
        Elements cols;
        for (int c = 0; c < level.cols; ++c)
            cols.push_back(RenderCell(level, r, c, scale));
        rows.push_back(hbox(std::move(cols)));
    }
    return vbox(std::move(rows));
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: sokoban <level-file>\n";
        return 1;
    }

    auto loaded = LoadLevel(argv[1]);
    if (!loaded) {
        std::cerr << "sokoban: failed to load level '" << argv[1]
                   << "' (missing, unreadable, or has no player '@')\n";
        return 1;
    }
    Level level = *loaded;
    int move_count = 0;

    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer([&] {
        int scale = ComputeScale(level, Terminal::Size());
        auto board = RenderBoard(level, scale) | border | center | flex;

        bool solved = IsSolved(level);
        std::string status = (solved ? std::string("Solved!") : std::string("arrow keys to move"))
                              + "   moves: " + std::to_string(move_count) + "   q to quit";

        return vbox({
            text(std::string("Sokoban — ") + argv[1]) | bold | center,
            separator(),
            board,
            separator(),
            text(status) | center,
        });
    });

    auto app = CatchEvent(renderer, [&](Event e) -> bool {
        int dr = 0, dc = 0;
        if (e == Event::ArrowUp) dr = -1;
        else if (e == Event::ArrowDown) dr = 1;
        else if (e == Event::ArrowLeft) dc = -1;
        else if (e == Event::ArrowRight) dc = 1;
        else if (e == Event::Character('q') || e == Event::Character('Q')) {
            screen.Exit();
            return true;
        } else {
            return false;
        }

        if (TryMove(level, dr, dc))
            ++move_count;
        return true;
    });

    screen.Loop(app);
}
