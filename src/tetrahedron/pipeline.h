#ifndef PIPELINE_H
#define PIPELINE_H

#include <ftxui/dom/canvas.hpp>

#include <camera.h>
#include <common/bouncing_scene.h>
#include <mesh.h>
#include <shading.h>
#include <transform.h>

// Sequences the pipeline for one frame: model transform -> per face
// (backface cull -> project -> shade -> rasterize) -> canvas. Visible faces
// never overlap in screen space for a convex solid under orthographic
// projection, so draw order among them doesn't affect correctness.
inline void render_frame(
    const common::Angles& angles,
    const common::Scene& scene,
    ftxui::Canvas& canvas) {
  double scale = nominal_scale(canvas.width(), canvas.height());
  WorldMesh world = transform(angles);

  for (int f = 0; f < 4; ++f) {
    if (not is_front_facing(world.normals[f])) continue;

    common::Point p0 =
        project(world.vertices[kFaces[f][0]], scene.cx, scene.cy, scale);
    common::Point p1 =
        project(world.vertices[kFaces[f][1]], scene.cx, scene.cy, scale);
    common::Point p2 =
        project(world.vertices[kFaces[f][2]], scene.cx, scene.cy, scale);

    common::fill_triangle(canvas, p0, p1, p2, shade_face(world.normals[f]));
  }
}

#endif  // PIPELINE_H
