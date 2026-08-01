#pragma once

#include <cmath>
#include <numbers>
#include <random>

#include "vec_math.h"

constexpr double kDegToRad = std::numbers::pi / 180.0;

struct Angles {
    double x = 0.0, y = 0.0, z = 0.0;
};

struct RotationRates {
    double x, y, z;
};

// Chosen once at startup: a rigid body spinning at a constant-but-random
// rate reads as tumbling, whereas re-randomizing the rate every tick would
// look like shaking instead.
inline RotationRates InitRotationRates(std::mt19937& rng) {
    std::uniform_real_distribution<double> mag(0.3 * kDegToRad, 1.0 * kDegToRad);
    std::uniform_int_distribution<int> sign(0, 1);
    auto pick = [&] {
        double m = mag(rng);
        return sign(rng) ? m : -m;
    };
    return {pick(), pick(), pick()};
}

// fmod-based wrap that stays correct for negative deltas, unlike a naive
// "subtract 2*pi once" wrap that only works when the angle always advances
// in the same direction (rotation rates here can be negative).
inline double WrapAngle(double a) {
    constexpr double kTwoPi = 2.0 * std::numbers::pi;
    a = std::fmod(a, kTwoPi);
    if (a < 0.0) a += kTwoPi;
    return a;
}

inline void Advance(Angles& a, const RotationRates& rates) {
    a.x = WrapAngle(a.x + rates.x);
    a.y = WrapAngle(a.y + rates.y);
    a.z = WrapAngle(a.z + rates.z);
}

// Rebuilt fresh from the persistent angles every call (rather than
// accumulated by repeated multiplication) to avoid orthonormality drift.
inline Mat3 BuildRotation(const Angles& a) {
    return RotationZ(a.z) * RotationY(a.y) * RotationX(a.x);
}
