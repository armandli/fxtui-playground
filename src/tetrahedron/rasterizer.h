#pragma once

#include <algorithm>
#include <cmath>

#include <ftxui/dom/canvas.hpp>
#include <ftxui/screen/color.hpp>

#include "camera.h"

namespace tetra_raster {

inline double EdgeSign(const Point& p1, const Point& p2, double px, double py) {
    return (px - p2.x) * (p1.y - p2.y) - (p1.x - p2.x) * (py - p2.y);
}

inline bool PointInTriangle(double px, double py, const Point& a, const Point& b, const Point& c) {
    double d1 = EdgeSign(a, b, px, py);
    double d2 = EdgeSign(b, c, px, py);
    double d3 = EdgeSign(c, a, px, py);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

}  // namespace tetra_raster

// Fills a 2D triangle onto the canvas with a flat color, using a
// bounding-box + edge-sign point-in-triangle test (ported from
// rotating_triangle's single-triangle renderer, generalized to take an
// arbitrary fill color so each face can be shaded independently).
inline void FillTriangle(ftxui::Canvas& canvas, const Point& p0, const Point& p1, const Point& p2,
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
            if (tetra_raster::PointInTriangle(x, y, p0, p1, p2)) {
                canvas.DrawPoint(x, y, true, color);
            }
        }
    }
}
