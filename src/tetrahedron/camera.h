#ifndef CAMERA_H
#define CAMERA_H

#include <algorithm>

#include <common/rasterizer2d.h>
#include <common/vec_math.h>
#include <mesh.h>

inline double nominal_scale(int canvas_w, int canvas_h) {
  return 0.32 * std::min(canvas_w, canvas_h) / kCircumradius;
}

// Projects a world-space vertex orthographically, offset by the scene's
// on-screen center. Y is flipped: +Y is "up" in model space but canvas rows
// grow downward.
inline common::Point project(
    const common::Vec3& world_vertex, double cx, double cy, double scale) {
  return {cx + scale * world_vertex.x, cy - scale * world_vertex.y};
}

// The camera sits on +Z looking toward -Z; with parallel orthographic view
// rays, "toward the camera" is the constant vector (0,0,1) everywhere, so
// visibility reduces to a sign check on the normal's Z component.
inline bool is_front_facing(const common::Vec3& world_normal) {
  return world_normal.z > 0.0;
}

#endif  // CAMERA_H
