#pragma once

#include <cmath>
#include <numbers>

namespace common {

constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kTwoPi = 2.0 * std::numbers::pi;

struct Angles {
    double x = 0.0, y = 0.0, z = 0.0;
};

// fmod-based wrap that stays correct for negative deltas, unlike a naive
// "subtract 2*pi once" wrap that only works when the angle always advances
// in the same direction.
inline double WrapAngle(double a) {
    a = std::fmod(a, kTwoPi);
    if (a < 0.0) a += kTwoPi;
    return a;
}

}  // namespace common
