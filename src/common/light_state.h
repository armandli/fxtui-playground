#ifndef COMMON_LIGHT_STATE_H
#define COMMON_LIGHT_STATE_H

#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

#include <common/vec_math.h>

namespace common {

// A light direction that starts at start_dir and drifts slowly via a random
// walk, hard-clamped to a cone around start_dir so the visible surface
// always has real directional lighting. An unconstrained random walk on a
// sphere is stationary-uniform in the long run and would eventually spend
// extended stretches lighting the far side, reading as a flat,
// ambient-only surface for minutes at a time.
struct LightState {
  Vec3 dir = normalize(Vec3{-0.5, 0.6, 0.65});
  Vec3 start_dir = dir;
  double drift_sigma = 0.02;
  double max_cone_angle = 75.0 * std::numbers::pi / 180.0;
};

inline void advance_light(LightState& light, std::mt19937& rng) {
  static std::normal_distribution<double> gauss(0.0, 1.0);
  Vec3 perturb{
      light.drift_sigma * gauss(rng),
      light.drift_sigma * gauss(rng),
      light.drift_sigma * gauss(rng),
  };
  Vec3 candidate = normalize(light.dir + perturb);

  double c = std::clamp(dot(candidate, light.start_dir), -1.0, 1.0);
  double angle = std::acos(c);
  if (angle > light.max_cone_angle) {
    Vec3 perp = candidate - light.start_dir * c;
    double len = length(perp);
    Vec3 perp_dir = (len > 1e-9) ? perp * (1.0 / len) : Vec3{0, 0, 0};
    light.dir = light.start_dir * std::cos(light.max_cone_angle) +
                perp_dir * std::sin(light.max_cone_angle);
  } else {
    light.dir = candidate;
  }
}

}  // namespace common

#endif  // COMMON_LIGHT_STATE_H
