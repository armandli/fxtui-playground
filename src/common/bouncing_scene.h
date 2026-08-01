#pragma once

#include <cmath>
#include <numbers>
#include <random>

namespace common {

// On-screen position and constant velocity of a point bouncing inside a
// width x height box.
struct Scene {
    double cx, cy;
    double vx, vy;
};

inline Scene InitScene(std::mt19937& rng, double cx0, double cy0, double speed) {
    std::uniform_real_distribution<double> angle(0.0, 2.0 * std::numbers::pi);
    double t = angle(rng);
    return {cx0, cy0, speed * std::cos(t), speed * std::sin(t)};
}

// Advances position by one tick and reflects velocity off the boundaries,
// clamping position to the boundary in the same step so it never tunnels
// past a wall.
inline void UpdateScene(Scene& s, int width, int height, double radius) {
    s.cx += s.vx;
    s.cy += s.vy;

    double lo_x = radius, hi_x = width - 1 - radius;
    double lo_y = radius, hi_y = height - 1 - radius;

    if (s.cx < lo_x) { s.cx = lo_x; s.vx = -s.vx; }
    if (s.cx > hi_x) { s.cx = hi_x; s.vx = -s.vx; }
    if (s.cy < lo_y) { s.cy = lo_y; s.vy = -s.vy; }
    if (s.cy > hi_y) { s.cy = hi_y; s.vy = -s.vy; }
}

}  // namespace common
