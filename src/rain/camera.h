#ifndef CAMERA_H
#define CAMERA_H

#include <common/angles.h>
#include <common/rasterizer2d.h>
#include <common/scene_radius.h>
#include <common/vec_math.h>

// Fraction of the cols x rows*2 "physics" grid the scene occupies -- see
// common/scene_radius.h. Larger than teapot's 0.30 since the ground fills
// more of the frame than a single centered solid.
constexpr double kRadiusFraction = 0.35;

// Normalization anchor: world geometry (ground.h, raindrop.h) is sized in
// ~this-scale units, so nominal_scale below does all terminal-size scaling
// uniformly, the same way teapot's kBoundingRadius/tetrahedron's
// kCircumradius anchor their own local-space geometry.
constexpr double kSceneBoundingRadius = 1.0;

// Must stay positive: the ground's world normal (0,1,0) is parallel to the
// Y axis, so rotation_y leaves it unchanged and rotation_x(kElevationAngle)
// turns it into (0, cos(elev), sin(elev)) regardless of azimuth --
// is_front_facing (normal.z > 0) requires sin(elev) > 0, i.e. the ground
// silently fails to render at elev <= 0.
constexpr double kElevationAngle = 25.0 * common::kDegToRad;

// Orbits the camera azimuthally around the scene at a fixed elevation,
// implemented the same way sphere/teapot fake object rotation: transform
// every world point by this single matrix rather than moving a separate
// camera. Yaw (orbit) first, then a fixed pitch tilt to look down at the
// ground from height.
inline common::Mat3 build_camera(double azimuth) {
  return common::rotation_x(kElevationAngle) * common::rotation_y(-azimuth);
}

// Single source of truth for the scene's screen-space radius, shared by
// nominal_scale below (sphere/teapot's compute_radius pattern).
inline double compute_radius(int cols, int rows) {
  return common::compute_scene_radius(cols, rows, kRadiusFraction);
}

inline double nominal_scale(int cols, int rows) {
  return compute_radius(cols, rows) / kSceneBoundingRadius;
}

// Projects a camera-space vertex orthographically into "physics" space
// (cols x rows*2, sphere's convention), offset by the scene's on-screen
// center. Y is flipped: +Y is "up" in camera space but physics-space rows
// grow downward.
inline common::Point project(
    const common::Vec3& camera_space_vertex, double cx, double cy,
    double scale) {
  return {
      cx + scale * camera_space_vertex.x, cy - scale * camera_space_vertex.y};
}

// The camera looks toward -Z in its own space; with parallel orthographic
// view rays, "toward the camera" is the constant vector (0,0,1) everywhere,
// so visibility reduces to a sign check on the camera-space normal's Z
// component.
inline bool is_front_facing(const common::Vec3& camera_space_normal) {
  return camera_space_normal.z > 0.0;
}

#endif  // CAMERA_H
