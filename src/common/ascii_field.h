#ifndef COMMON_ASCII_FIELD_H
#define COMMON_ASCII_FIELD_H

#include <array>
#include <vector>

#include <common/glyphs.h>
#include <common/sample_circle.h>

namespace common {

// Per-cell brightness samples at the 6 kSampleCircles positions (0.0 = no
// coverage/miss), row-major over a cols x rows grid.
using AsciiField = std::vector<std::array<double, 6>>;

// Turns a raw brightness field into the best-matching ASCII character per
// cell: contrast enhancement, then a directional fold across each cell's
// shared vertical edges (so a bright neighbor sharpens a dim edge sample
// before matching), then nearest-neighbor glyph matching. Shape-agnostic --
// any renderer that can fill in six samples per cell at the
// common::kSampleCircles positions can reuse this, whether the underlying
// surface is analytic (sphere) or a rasterized triangle mesh (teapot).
inline std::vector<char> ascii_from_field(
    AsciiField field, int cols, int rows) {
  for (auto& v : field) apply_global_contrast(v);

  std::vector<char> output(static_cast<size_t>(cols) * rows);
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      // copy: folding must not mutate a neighbor's stored value
      auto v = field[row * cols + col];
      if (col > 0) {
        v[0] = directional_fold(v[0], field[row * cols + (col - 1)][5]);
      }
      if (col < cols - 1) {
        v[5] = directional_fold(v[5], field[row * cols + (col + 1)][0]);
      }
      output[row * cols + col] = match_char(v);
    }
  }

  return output;
}

}  // namespace common

#endif  // COMMON_ASCII_FIELD_H
