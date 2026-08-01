#pragma once

#include <algorithm>
#include <fstream>
#include <optional>
#include <string>
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
    return a.row == b.row && a.col == b.col;
}

enum class Cell { Wall, Floor, Goal };

struct Level {
    std::vector<std::vector<Cell>> grid;  // base terrain only, boxes/player tracked separately
    std::vector<Position> boxes;
    Position player;
    int rows = 0;
    int cols = 0;
};

inline bool InBounds(const Level& level, Position p) {
    return p.row >= 0 && p.row < level.rows && p.col >= 0 && p.col < level.cols;
}

inline Cell At(const Level& level, Position p) {
    return level.grid[p.row][p.col];
}

inline bool HasBoxAt(const Level& level, Position p) {
    return std::find(level.boxes.begin(), level.boxes.end(), p) != level.boxes.end();
}

// Loads a level from `path`. Returns std::nullopt if the file can't be opened
// or contains no player.
inline std::optional<Level> LoadLevel(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return std::nullopt;

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        lines.push_back(line);
    }

    size_t width = 0;
    for (const auto& l : lines)
        width = std::max(width, l.size());
    if (lines.empty() || width == 0)
        return std::nullopt;

    Level level;
    level.rows = static_cast<int>(lines.size());
    level.cols = static_cast<int>(width);
    level.grid.assign(level.rows, std::vector<Cell>(level.cols, Cell::Floor));

    bool found_player = false;
    for (int r = 0; r < level.rows; ++r) {
        const std::string& l = lines[r];
        for (int c = 0; c < level.cols; ++c) {
            char ch = c < static_cast<int>(l.size()) ? l[c] : ' ';
            Position pos{r, c};
            switch (ch) {
                case '#':
                    level.grid[r][c] = Cell::Wall;
                    break;
                case '.':
                    level.grid[r][c] = Cell::Goal;
                    break;
                case '$':
                    level.boxes.push_back(pos);
                    break;
                case '*':
                    level.grid[r][c] = Cell::Goal;
                    level.boxes.push_back(pos);
                    break;
                case '@':
                    level.player = pos;
                    found_player = true;
                    break;
                case '+':
                    level.grid[r][c] = Cell::Goal;
                    level.player = pos;
                    found_player = true;
                    break;
                default:
                    break;  // floor
            }
        }
    }

    if (!found_player)
        return std::nullopt;

    return level;
}

// Attempts to move the player by (dr, dc) — one of the four cardinal
// directions. A box in the destination cell is pushed one cell further in
// the same direction if (and only if) that cell is in bounds, not a wall,
// and not occupied by another box. Returns true if the player (and possibly
// a box) moved.
inline bool TryMove(Level& level, int dr, int dc) {
    Position target{level.player.row + dr, level.player.col + dc};
    if (!InBounds(level, target) || At(level, target) == Cell::Wall)
        return false;

    if (HasBoxAt(level, target)) {
        Position beyond{target.row + dr, target.col + dc};
        if (!InBounds(level, beyond) || At(level, beyond) == Cell::Wall || HasBoxAt(level, beyond))
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

inline bool IsSolved(const Level& level) {
    for (const auto& box : level.boxes) {
        if (At(level, box) != Cell::Goal)
            return false;
    }
    return true;
}
