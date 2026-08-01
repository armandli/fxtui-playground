#include <iostream>
#include <string>

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <common/grid_render.h>
#include <level.h>

using namespace ftxui;

namespace {

// Each game cell renders as `scale` terminal rows tall and `2*scale` columns
// wide (double width so cells read roughly square despite typical terminal
// font proportions).
int compute_scale(const Level& level, Dimensions term) {
  // Overhead outside the board: title(1) + sep(1) + sep(1) + status(1)
  // + board border(2 rows, 2 cols) = 6 rows, 2 cols.
  int avail_rows = std::max(1, term.dimy - 6);
  int avail_cols = std::max(1, term.dimx - 2);
  return common::compute_grid_scale(
      avail_rows,
      avail_cols,
      level.rows,
      level.cols);
}

Element render_cell(const Level& level, int r, int c, int scale) {
  Position pos{r, c};
  Cell base = at(level, pos);
  bool is_player = level.player == pos;
  bool has_box = has_box_at(level, pos);

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

  return common::render_glyph_cell(bg, fg, glyph, scale);
}

Element render_board(const Level& level, int scale) {
  return common::render_grid(level.rows, level.cols, [&](int r, int c) {
    return render_cell(level, r, c, scale);
  });
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "usage: sokoban <level-file>\n";
    return 1;
  }

  auto loaded = load_level(argv[1]);
  if (not loaded) {
    std::cerr << "sokoban: failed to load level '" << argv[1]
               << "' (missing, unreadable, or has no player '@')\n";
    return 1;
  }
  Level level = *loaded;
  int move_count = 0;

  auto screen = ScreenInteractive::Fullscreen();

  auto renderer = Renderer([&] {
    int scale = compute_scale(level, Terminal::Size());
    auto board = render_board(level, scale) | border | center | flex;

    bool solved = is_solved(level);
    std::string status =
        (solved ? std::string("Solved!") : std::string("arrow keys to move"))
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
    else if (e == Event::Character('q') or e == Event::Character('Q')) {
      screen.Exit();
      return true;
    } else {
      return false;
    }

    if (try_move(level, dr, dc))
      ++move_count;
    return true;
  });

  screen.Loop(app);
}
