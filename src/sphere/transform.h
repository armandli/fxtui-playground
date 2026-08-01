#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <random>

#include <common/angles.h>
#include <common/vec_math.h>

struct RotationRates {
  double x, y, z;
};

// Chosen once at startup: a rigid body spinning at a constant-but-random
// rate reads as tumbling, whereas re-randomizing the rate every tick would
// look like shaking instead.
inline RotationRates init_rotation_rates(std::mt19937& rng) {
  std::uniform_real_distribution<double> mag(
      0.3 * common::kDegToRad, 1.0 * common::kDegToRad);
  std::uniform_int_distribution<int> sign(0, 1);
  auto pick = [&] {
    double m = mag(rng);
    return sign(rng) ? m : -m;
  };
  return {pick(), pick(), pick()};
}

inline void advance(common::Angles& a, const RotationRates& rates) {
  a.x = common::wrap_angle(a.x + rates.x);
  a.y = common::wrap_angle(a.y + rates.y);
  a.z = common::wrap_angle(a.z + rates.z);
}

// Rebuilt fresh from the persistent angles every call (rather than
// accumulated by repeated multiplication) to avoid orthonormality drift.
inline common::Mat3 build_rotation(const common::Angles& a) {
  return common::rotation_z(a.z) * common::rotation_y(a.y) *
         common::rotation_x(a.x);
}

#endif  // TRANSFORM_H
