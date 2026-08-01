#ifndef LEVEL_H
#define LEVEL_H

#include <algorithm>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Sokoban level model, file parsing, and move rules.
//
// Text format (the de-facto standard Sokoban notation):
//   #  wall
//   (space) floor
//   .  goal
//   $  box
//   *  box on goal
//   @  player
//   +  player on goal
// Lines may be ragged; shorter lines are padded with floor on load.

struct Position {
  int row = 0;
  int col = 0;
};

inline bool operator==(const Position& a, const Position& b) {
  return a.row == b.row and a.col == b.col;
}

enum class Cell : int {
  Wall,
  Floor,
  Goal,
};

struct Level {
  std::vector<std::vector<Cell>> grid;  // base terrain; boxes/player tracked separately
  std::vector<Position> boxes;
  Position player;
  int rows = 0;
  int cols = 0;
};

inline bool in_bounds(const Level& level, Position p) {
  return p.row >= 0 and p.row < level.rows and p.col >= 0 and p.col < level.cols;
}

inline Cell at(const Level& level, Position p) {
  return level.grid[p.row][p.col];
}

inline bool has_box_at(const Level& level, Position p) {
  auto it = std::find(level.boxes.begin(), level.boxes.end(), p);
  return it != level.boxes.end();
}

// Loads a level from `path`. Returns std::nullopt if the file can't be opened
// or contains no player.
inline std::optional<Level> load_level(const std::string& path) {
  std::ifstream file(path);
  if (not file.is_open())
    return std::nullopt;

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    while (not line.empty() and (line.back() == '\r' or line.back() == '\n'))
      line.pop_back();
    lines.push_back(line);
  }

  size_t width = 0;
  for (const auto& l : lines)
    width = std::max(width, l.size());
  if (lines.empty() or width == 0)
    return std::nullopt;

  Level level;
  level.rows = static_cast<int>(lines.size());
  level.cols = static_cast<int>(width);
  level.grid.assign(level.rows, std::vector<Cell>(level.cols, Cell::Floor));

  bool found_player = false;
  for (int r = 0; r < level.rows; ++r) {
    std::string_view l = lines[r];
    for (int c = 0; c < level.cols; ++c) {
      char ch = c < static_cast<int>(l.size()) ? l[c] : ' ';
      Position pos{r, c};
      switch (ch) {
        case '#':
          level.grid[r][c] = Cell::Wall;

        break; case '.':
          level.grid[r][c] = Cell::Goal;

        break; case '$':
          level.boxes.push_back(pos);

        break; case '*':
          level.grid[r][c] = Cell::Goal;
          level.boxes.push_back(pos);

        break; case '@':
          level.player = pos;
          found_player = true;

        break; case '+':
          level.grid[r][c] = Cell::Goal;
          level.player = pos;
          found_player = true;

        break; default:
          ;  // floor
      }
    }
  }

  if (not found_player)
    return std::nullopt;

  return level;
}

// Attempts to move the player by (dr, dc) — one of the four cardinal
// directions. A box in the destination cell is pushed one cell further in
// the same direction if (and only if) that cell is in bounds, not a wall,
// and not occupied by another box. Returns true if the player (and possibly
// a box) moved.
inline bool try_move(Level& level, int dr, int dc) {
  Position target{level.player.row + dr, level.player.col + dc};
  if (not in_bounds(level, target) or at(level, target) == Cell::Wall)
    return false;

  if (has_box_at(level, target)) {
    Position beyond{target.row + dr, target.col + dc};
    if (not in_bounds(level, beyond) or at(level, beyond) == Cell::Wall
        or has_box_at(level, beyond))
      return false;
    for (auto& box : level.boxes) {
      if (box == target) {
        box = beyond;
        break;
      }
    }
  }

  level.player = target;
  return true;
}

inline bool is_solved(const Level& level) {
  for (const auto& box : level.boxes) {
    if (at(level, box) != Cell::Goal)
      return false;
  }
  return true;
}

#endif  // LEVEL_H
