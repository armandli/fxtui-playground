#ifndef RASTERIZER2D_H
#define RASTERIZER2D_H

#include <algorithm>
#include <cmath>

#include <ftxui/dom/canvas.hpp>
#include <ftxui/screen/color.hpp>

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
