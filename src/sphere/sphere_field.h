#ifndef SPHERE_FIELD_H
#define SPHERE_FIELD_H

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>

#include <common/lighting.h>
#include <common/vec_math.h>

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

  return common::shade(normal, light_dir, albedo);
}

#endif  // SPHERE_FIELD_H
