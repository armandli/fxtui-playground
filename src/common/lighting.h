#ifndef COMMON_LIGHTING_H
#define COMMON_LIGHTING_H

#include <algorithm>
#include <cmath>

#include <common/vec_math.h>

namespace common {

constexpr double kDefaultAmbient = 0.20;
constexpr double kDefaultSpecStrength = 0.35;
constexpr double kDefaultShininess = 24.0;

// Ambient + Lambertian diffuse (scaled by albedo, so surface texture stays
// faintly visible even in dim regions) plus a Blinn-Phong specular
// highlight (left unscaled by albedo, since real specular highlights are
// near-independent of surface color) -- the highlight is what reads as a
// "shiny 3D solid" rather than a flat shaded surface at only a handful of
// discrete ASCII brightness levels. Assumes an orthographic camera looking
// down +Z, so "toward the camera" is the constant vector (0, 0, 1).
inline double shade(
    const Vec3& normal,
    const Vec3& light_dir,
    double albedo,
    double ambient = kDefaultAmbient,
    double spec_strength = kDefaultSpecStrength,
    double shininess = kDefaultShininess) {
  double diffuse = std::max(0.0, dot(normal, light_dir));
  double base = ambient + (1.0 - ambient) * diffuse;

  Vec3 half_vec = normalize(light_dir + Vec3{0, 0, 1});
  double spec_angle = std::max(0.0, dot(normal, half_vec));
  double spec = spec_strength * std::pow(spec_angle, shininess);

  return std::clamp(albedo * base + spec, 0.0, 1.0);
}

}  // namespace common

#endif  // COMMON_LIGHTING_H
