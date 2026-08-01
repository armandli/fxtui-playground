#ifndef SPHERE_FIELD_H
#define SPHERE_FIELD_H

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <random>

#include <common/vec_math.h>

// Light direction random walk: starts top-left-biased and drifts slowly,
// hard-clamped to a cone around the start direction so the visible
// hemisphere always has real directional lighting. An unconstrained random
// walk on a sphere is stationary-uniform in the long run and would
// eventually spend extended stretches lighting the far side, reading as a
// flat, ambient-only disc for minutes at a time.
inline const common::Vec3 kLightStart =
    common::normalize(common::Vec3{-0.5, 0.6, 0.65});
constexpr double kLightDriftSigma = 0.02;
constexpr double kMaxLightConeAngle = 75.0 * std::numbers::pi / 180.0;

struct LightState {
  common::Vec3 dir = kLightStart;
};

inline void advance_light(LightState& light, std::mt19937& rng) {
  static std::normal_distribution<double> gauss(0.0, 1.0);
  common::Vec3 perturb{
      kLightDriftSigma * gauss(rng),
      kLightDriftSigma * gauss(rng),
      kLightDriftSigma * gauss(rng),
  };
  common::Vec3 candidate = common::normalize(light.dir + perturb);

  double c = std::clamp(common::dot(candidate, kLightStart), -1.0, 1.0);
  double angle = std::acos(c);
  if (angle > kMaxLightConeAngle) {
    common::Vec3 perp = candidate - kLightStart * c;
    double len = common::length(perp);
    common::Vec3 perp_dir =
        (len > 1e-9) ? perp * (1.0 / len) : common::Vec3{0, 0, 0};
    light.dir = kLightStart * std::cos(kMaxLightConeAngle)
                + perp_dir * std::sin(kMaxLightConeAngle);
  } else {
    light.dir = candidate;
  }
}

// Latitude-band surface texture so the sphere's own rotation is visible --
// a smooth featureless sphere is rotationally symmetric and would look
// static while spinning.
constexpr int kNumLatBands = 6;
constexpr double kBandAlbedoLow = 0.85;
constexpr double kBandAlbedoHigh = 1.0;

inline double latitude_albedo(const common::Vec3& local_point) {
  double lat = std::asin(std::clamp(local_point.y, -1.0, 1.0));
  double t = (lat + std::numbers::pi / 2.0) / std::numbers::pi;
  int band = std::min(kNumLatBands - 1, static_cast<int>(t * kNumLatBands));
  return (band % 2 == 0) ? kBandAlbedoLow : kBandAlbedoHigh;
}

constexpr double kAmbient = 0.20;
constexpr double kSpecStrength = 0.35;
constexpr double kShininess = 24.0;

// Ambient + Lambertian diffuse (scaled by albedo, so latitude bands stay
// faintly visible even in dim regions) plus a Blinn-Phong specular
// highlight (left unscaled by albedo, since real specular highlights are
// near-independent of surface color) -- the highlight is what reads as a
// "shiny 3D solid" rather than a flat shaded disc at only 10 discrete
// ASCII brightness levels.
inline double shade(
    const common::Vec3& normal,
    const common::Vec3& light_dir,
    double albedo) {
  double diffuse = std::max(0.0, common::dot(normal, light_dir));
  double base = kAmbient + (1.0 - kAmbient) * diffuse;

  common::Vec3 half_vec =
      common::normalize(light_dir + common::Vec3{0, 0, 1});
  double spec_angle = std::max(0.0, common::dot(normal, half_vec));
  double spec = kSpecStrength * std::pow(spec_angle, kShininess);

  return std::clamp(albedo * base + spec, 0.0, 1.0);
}

// Analytic ray-sphere test in physics-space offsets (dx, dy) from the
// sphere's screen-space center, for a unit sphere of the given radius.
// Orthographic camera on +Z, so the local unit-sphere point *is* the
// world-space point/normal (front hemisphere, already unit length) -- this
// must never be multiplied by the body's own rotation matrix, or lighting
// would spin with the body instead of staying fixed relative to the
// independently-drifting light.
inline std::optional<double> shade_sample(
    double dx,
    double dy,
    double radius,
    const common::Mat3& body_rotation,
    const common::Vec3& light_dir) {
  double x = dx / radius;
  // physics-space y grows downward; world convention is +Y = up
  double y = -dy / radius;
  double r2 = x * x + y * y;
  if (r2 > 1.0) return std::nullopt;

  double z = std::sqrt(1.0 - r2);
  common::Vec3 normal{x, y, z};

  // Un-spin *only* for the texture lookup, via R^T (== R^-1 for a
  // rotation matrix), so the bands stay attached to the body's own
  // rotating surface instead of the fixed camera/light frame.
  common::Vec3 local_point = common::transpose(body_rotation) * normal;
  double albedo = latitude_albedo(local_point);

  return shade(normal, light_dir, albedo);
}

#endif  // SPHERE_FIELD_H
