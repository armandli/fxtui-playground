#pragma once

#include <random>

#include "common/angles.h"
#include "common/vec_math.h"

using namespace common;

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
