#ifndef PIPELINE_H
#define PIPELINE_H

#include <array>
#include <vector>

#include <common/ascii_field.h>
#include <common/bouncing_scene.h>
#include <common/light_state.h>
#include <common/rotation_state.h>
#include <common/sample_circle.h>
#include <common/scene_radius.h>
#include <sphere_field.h>

// Smaller than a comparable prior app's 0.32 fraction so the sphere
// doesn't fill the frame and bounce motion stays clearly visible.
constexpr double kRadiusFraction = 0.28;

// Single source of truth for the sphere's screen-space radius, used both
// by render_frame below and by the bounce-physics update in main.cpp so
// they never disagree on the play field.
inline double compute_radius(int cols, int rows) {
  return common::compute_scene_radius(cols, rows, kRadiusFraction);
}

// Fills the per-cell 6-sample brightness field from the analytic
// ray-sphere test, then hands it to the shape-agnostic common ASCII
// matcher (contrast enhancement, directional fold, glyph matching).
inline std::vector<char> render_frame(
    const common::Angles& angles,
    const common::Scene& scene,
    const common::LightState& light,
    int cols,
    int rows) {
  double radius = compute_radius(cols, rows);
  common::Mat3 body_rotation = common::build_rotation(angles);

  common::AsciiField field(static_cast<size_t>(cols) * rows);

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      auto& v = field[row * cols + col];
      for (int k = 0; k < 6; ++k) {
        const auto& circle = common::kSampleCircles[k];
        double px = col + circle.x;
        double py = row * 2.0 + circle.y;
        double dx = px - scene.cx;
        double dy = py - scene.cy;
        auto result = shade_sample(dx, dy, radius, body_rotation, light.dir);
        v[k] = result.value_or(0.0);
      }
    }
  }

  return common::ascii_from_field(std::move(field), cols, rows);
}

#endif  // PIPELINE_H
