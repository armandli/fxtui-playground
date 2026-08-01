#ifndef PIPELINE_H
#define PIPELINE_H

#include <vector>

#include <camera.h>
#include <common/ascii_field.h>
#include <common/bouncing_scene.h>
#include <common/light_state.h>
#include <raster_field.h>
#include <transform.h>

// Per-frame render: rotate the cached local mesh -> rasterize into a
// per-subsample brightness field (with hidden-surface removal) -> hand the
// field to the shape-agnostic common ASCII matcher (contrast enhancement,
// directional fold, glyph matching).
inline std::vector<char> render_frame(
    const common::Angles& angles,
    const common::Scene& scene,
    const common::LightState& light,
    int cols,
    int rows) {
  double scale = nominal_scale(cols, rows);
  WorldMesh world = transform(angles);

  common::AsciiField field =
      raster_field(world, light, scene.cx, scene.cy, scale, cols, rows);
  return common::ascii_from_field(std::move(field), cols, rows);
}

#endif  // PIPELINE_H
