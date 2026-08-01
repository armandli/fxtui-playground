#ifndef RASTERIZER2D_H
#define RASTERIZER2D_H

#include <algorithm>
#include <cmath>
#include <optional>

#include <ftxui/dom/canvas.hpp>
#include <ftxui/screen/color.hpp>

#include <common/vec_math.h>

namespace common {

struct Point {
  double x, y;
};

// Sign of the cross product (p2-p1) x (p-p1); used for the point-in-triangle test.
inline double edge_sign(
    const Point& p1, const Point& p2, double px, double py) {
  return (px - p2.x) * (p1.y - p2.y) - (p1.x - p2.x) * (py - p2.y);
}

inline bool point_in_triangle(
    double px,
    double py,
    const Point& a,
    const Point& b,
    const Point& c) {
  double d1 = edge_sign(a, b, px, py);
  double d2 = edge_sign(b, c, px, py);
  double d3 = edge_sign(c, a, px, py);
  bool has_neg = (d1 < 0) or (d2 < 0) or (d3 < 0);
  bool has_pos = (d1 > 0) or (d2 > 0) or (d3 > 0);
  return not (has_neg and has_pos);
}

// Barycentric weights (wa, wb, wc) of (px, py) with respect to triangle
// (a, b, c), packed into a Vec3 -- reusing Vec3 as a plain 3-tuple rather
// than introducing a single-purpose struct. edge_sign(a,b,.)/(b,c,.)/(c,a,.)
// are each proportional to the area swept opposite one vertex, and always
// sum to the triangle's own (signed) area regardless of winding, so dividing
// through by that sum yields weights that are valid for either winding
// order. Returns nullopt if (px, py) is outside the triangle or the
// triangle is degenerate (zero area).
inline std::optional<Vec3> barycentric(
    const Point& a, const Point& b, const Point& c, double px, double py) {
  double d1 = edge_sign(a, b, px, py);
  double d2 = edge_sign(b, c, px, py);
  double d3 = edge_sign(c, a, px, py);
  double total = d1 + d2 + d3;
  if (std::abs(total) < 1e-12) return std::nullopt;

  double wa = d2 / total, wb = d3 / total, wc = d1 / total;
  if (wa < 0.0 or wb < 0.0 or wc < 0.0) return std::nullopt;
  return Vec3{wa, wb, wc};
}

// Fills a 2D triangle onto the canvas with a flat color, using a
// bounding-box + edge-sign point-in-triangle test.
inline void fill_triangle(
    ftxui::Canvas& canvas,
    const Point& p0,
    const Point& p1,
    const Point& p2,
    const ftxui::Color& color) {
  int min_x = static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x})));
  int max_x = static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x})));
  int min_y = static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y})));
  int max_y = static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y})));

  min_x = std::clamp(min_x, 0, canvas.width() - 1);
  max_x = std::clamp(max_x, 0, canvas.width() - 1);
  min_y = std::clamp(min_y, 0, canvas.height() - 1);
  max_y = std::clamp(max_y, 0, canvas.height() - 1);

  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x; ++x) {
      if (point_in_triangle(x, y, p0, p1, p2)) {
        canvas.DrawPoint(x, y, true, color);
      }
    }
  }
}

}  // namespace common

#endif  // RASTERIZER2D_H
