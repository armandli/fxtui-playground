#ifndef COMMON_ROTATION_STATE_H
#define COMMON_ROTATION_STATE_H

#include <random>

#include <common/angles.h>
#include <common/vec_math.h>

namespace common {

struct RotationRates {
  double x, y, z;
};

// Chosen once at startup: a rigid body spinning at a constant-but-random
// rate reads as tumbling, whereas re-randomizing the rate every tick would
// look like shaking instead.
inline RotationRates init_rotation_rates(
    std::mt19937& rng, double min_deg_per_tick = 0.3,
    double max_deg_per_tick = 1.0) {
  std::uniform_real_distribution<double> mag(
      min_deg_per_tick * kDegToRad, max_deg_per_tick * kDegToRad);
  std::uniform_int_distribution<int> sign(0, 1);
  auto pick = [&] {
    double m = mag(rng);
    return sign(rng) ? m : -m;
  };
  return {pick(), pick(), pick()};
}

inline void advance_angles(Angles& a, const RotationRates& rates) {
  a.x = wrap_angle(a.x + rates.x);
  a.y = wrap_angle(a.y + rates.y);
  a.z = wrap_angle(a.z + rates.z);
}

// Rebuilt fresh from the persistent angles every call (rather than
// accumulated by repeated multiplication) to avoid orthonormality drift.
inline Mat3 build_rotation(const Angles& a) {
  return rotation_z(a.z) * rotation_y(a.y) * rotation_x(a.x);
}

}  // namespace common

#endif  // COMMON_ROTATION_STATE_H
