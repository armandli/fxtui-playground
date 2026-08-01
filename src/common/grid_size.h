#ifndef COMMON_GRID_SIZE_H
#define COMMON_GRID_SIZE_H

#include <algorithm>

#include <ftxui/screen/terminal.hpp>

namespace common {

struct GridSize {
  int cols, rows;
};

// Cell-grid size available inside a bordered, titled vbox: title(1) +
// separator(1) + border top/bottom(2) rows, border left/right(2) cols. The
// overhead convention shared by every "text-grid" ASCII renderer (sphere,
// teapot) in this repo.
inline GridSize compute_grid_size(ftxui::Dimensions term) {
  int cols = std::max(10, term.dimx - 2);
  int rows = std::max(4, term.dimy - 4);
  return {cols, rows};
}

}  // namespace common

#endif  // COMMON_GRID_SIZE_H
