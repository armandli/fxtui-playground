#ifndef PIPELINE_H
#define PIPELINE_H

#include <array>
#include <utility>
#include <vector>

#include <ftxui/screen/color.hpp>

#include <common/ascii_field.h>
#include <scene_field.h>

inline const ftxui::Color kDarkGreen = ftxui::Color::RGB(10, 60, 10);
inline const ftxui::Color kBrightGreen = ftxui::Color::RGB(80, 230, 80);
inline const ftxui::Color kDarkWhite = ftxui::Color::RGB(140, 140, 140);
inline const ftxui::Color kBrightWhite = ftxui::Color::RGB(255, 255, 255);

struct RenderedFrame {
  std::vector<char> chars;
  std::vector<ftxui::Color> colors;  // only meaningful where chars[i] != ' '
};

// Per-cell dominant material by brightness-weighted vote, not a plain
// subsample count: a thin raindrop streak typically only wins 1-3 of a
// cell's 6 subsamples, so a count-based majority would render it in the
// ground's color even though it visibly shaped the cell's glyph. Summing
// brightness per material instead keeps color consistent with what
// actually contributed to the shape match ascii_from_field is about to
// make. Returns the winning material and its mean subsample brightness
// (used as the color-interpolation intensity).
inline std::pair<Material, double> dominant_material(
    const std::array<double, 6>& brightness,
    const std::array<Material, 6>& material) {
  double rain_ink = 0.0, ground_ink = 0.0;
  int rain_n = 0, ground_n = 0;
  for (int k = 0; k < 6; ++k) {
    if (material[k] == Material::Rain) {
      rain_ink += brightness[k];
      ++rain_n;
    } else if (material[k] == Material::Ground) {
      ground_ink += brightness[k];
      ++ground_n;
    }
  }
  if (rain_ink == 0.0 and ground_ink == 0.0) return {Material::None, 0.0};
  if (rain_ink >= ground_ink) return {Material::Rain, rain_ink / rain_n};
  return {Material::Ground, ground_ink / ground_n};
}

// Per-frame render: rasterize ground + raindrops into a combined field ->
// pick each cell's color from a brightness-weighted material vote ->
// hand the same raw brightness field to the shape-agnostic common ASCII
// matcher (contrast enhancement, directional fold, glyph matching) for the
// characters, unchanged from how sphere/teapot use it.
inline RenderedFrame render_frame(
    const std::vector<Drop>& drops, double azimuth, int cols, int rows) {
  SceneField scene = rasterize_scene(drops, azimuth, cols, rows);

  size_t cell_count = static_cast<size_t>(cols) * rows;
  RenderedFrame frame;
  frame.colors.resize(cell_count);
  for (size_t i = 0; i < cell_count; ++i) {
    auto [dominant, intensity] =
        dominant_material(scene.brightness[i], scene.material[i]);
    if (dominant == Material::Rain) {
      frame.colors[i] = ftxui::Color::Interpolate(
          static_cast<float>(intensity), kDarkGreen, kBrightGreen);
    } else if (dominant == Material::Ground) {
      frame.colors[i] = ftxui::Color::Interpolate(
          static_cast<float>(intensity), kDarkWhite, kBrightWhite);
    } else {
      frame.colors[i] = ftxui::Color::Default;
    }
  }

  frame.chars =
      common::ascii_from_field(std::move(scene.brightness), cols, rows);
  return frame;
}

#endif  // PIPELINE_H
