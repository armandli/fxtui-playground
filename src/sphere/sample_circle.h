#pragma once

#include <algorithm>
#include <array>
#include <cmath>

struct SampleCircle {
    double x, y, r;
};

// 3 columns x 2 rows, staggered by a half row-pitch (left column pushed
// down, right column pushed up) so the six ~0.30-radius circles tile a
// cell without gaps despite its ~1:2 aspect ratio (x spans [0,1], y spans
// [0,2] -- this y range lines up 1:1 with scene.h's phys_h = rows*2
// convention). Indices 0 and 5 sit on the vertical midline and are the
// only two that spill purely horizontally into a neighboring cell -- the
// two indices the directional contrast pass below operates on. The other
// two outer circles land exactly on the top/bottom edges and spill into a
// corner (two edges plus the diagonal neighbor); left unhandled as a
// deliberately accepted minor approximation.
inline constexpr std::array<SampleCircle, 6> kSampleCircles = {{
    {1.0 / 6.0, 1.0, 0.30},  // 0: MidLeft  -- spills into the left neighbor
    {0.5, 0.5, 0.30},        // 1: TopMid
    {5.0 / 6.0, 0.0, 0.30},  // 2: TopRight -- corner spill, unhandled
    {1.0 / 6.0, 2.0, 0.30},  // 3: BotLeft  -- corner spill, unhandled
    {0.5, 1.5, 0.30},        // 4: BotMid
    {5.0 / 6.0, 1.0, 0.30},  // 5: MidRight -- spills into the right neighbor
}};

constexpr double kContrastExponent = 1.6;

// Pulls dark components toward zero more aggressively than bright ones,
// relative to the vector's own brightest component.
inline void ApplyGlobalContrast(std::array<double, 6>& v, double exponent = kContrastExponent) {
    double max_v = *std::max_element(v.begin(), v.end());
    if (max_v <= 1e-9) return;
    for (double& x : v) x = max_v * std::pow(x / max_v, exponent);
}

// Sharpens the transition across a shared cell edge: a strong neighbor
// sample can "pull up" a weak local one before the same power-curve is
// applied, using their shared max as the new brightest reference.
inline double DirectionalFold(double local, double neighbor, double exponent = kContrastExponent) {
    double max_v = std::max(local, neighbor);
    if (max_v <= 1e-9) return 0.0;
    return max_v * std::pow(local / max_v, exponent);
}
