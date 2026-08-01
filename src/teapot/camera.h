#ifndef CAMERA_H
#define CAMERA_H

#include <common/rasterizer2d.h>
#include <common/scene_radius.h>
#include <common/vec_math.h>
#include <teapot_control_points.h>

// Sized against the coarser cols x rows*2 "physics" grid (sphere's
// convention) rather than tetrahedron's 2x/4x-supersampled Canvas.
constexpr double kRadiusFraction = 0.30;

// Single source of truth for the teapot's screen-space radius, used both by
// nominal_scale below and by the bounce-physics update in main.cpp so they
// never disagree on the play field (sphere's compute_radius pattern).
inline double compute_radius(int cols, int rows) {
  return common::compute_scene_radius(cols, rows, kRadiusFraction);
}

inline double nominal_scale(int cols, int rows) {
  return compute_radius(cols, rows) / kBoundingRadius;
}

// Projects a world-space vertex orthographically into "physics" space
// (cols x rows*2, sphere's convention), offset by the scene's on-screen
// center. Y is flipped: +Y is "up" in model space but physics-space rows
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
