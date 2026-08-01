#ifndef SHADING_H
#define SHADING_H

#include <algorithm>

#include <ftxui/screen/color.hpp>

#include <common/vec_math.h>

// Light from the top-left (-X, +Y), biased toward the camera (+Z) so
// straight-on faces aren't left nearly unlit.
inline const common::Vec3 kLightDir =
    common::normalize(common::Vec3{-0.5, 0.6, 0.65});

constexpr double kAmbient = 0.25;

inline const ftxui::Color kDarkGreen = ftxui::Color::RGB(10, 50, 10);
inline const ftxui::Color kBrightGreen = ftxui::Color::RGB(70, 230, 70);

// Flat Lambertian shading: ambient floor plus a diffuse term from the angle
// between the face normal and the light direction.
inline ftxui::Color shade_face(const common::Vec3& world_normal) {
  double diffuse = std::max(0.0, common::dot(world_normal, kLightDir));
  double intensity =
      std::clamp(kAmbient + (1.0 - kAmbient) * diffuse, 0.0, 1.0);
  return ftxui::Color::Interpolate(
      static_cast<float>(intensity), kDarkGreen, kBrightGreen);
}

#endif  // SHADING_H
