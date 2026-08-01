#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <array>

#include <common/angles.h>
#include <common/vec_math.h>
#include <mesh.h>

// Slow, independent-rate rotation on all three axes so the tetrahedron reads
// as genuinely tumbling rather than spinning flat around one edge-on axis.
constexpr double kStepX = 0.9 * common::kDegToRad;
constexpr double kStepY = 0.6 * common::kDegToRad;
constexpr double kStepZ = 0.4 * common::kDegToRad;

inline void advance(common::Angles& a) {
  a.x = common::wrap_angle(a.x + kStepX);
  a.y = common::wrap_angle(a.y + kStepY);
  a.z = common::wrap_angle(a.z + kStepZ);
}

struct WorldMesh {
  std::array<common::Vec3, 4> vertices;
  std::array<common::Vec3, 4> normals;
};

// Rebuilds the rotation matrix fresh from the persistent angles every call
// (rather than accumulating it by repeated multiplication) to avoid
// orthonormality drift over long runs.
inline WorldMesh transform(const common::Angles& a) {
  common::Mat3 r = common::rotation_z(a.z) * common::rotation_y(a.y) *
                   common::rotation_x(a.x);
  const auto& base_normals = local_normals();

  WorldMesh w;
  for (int i = 0; i < 4; ++i) w.vertices[i] = r * kLocalVertices[i];
  for (int f = 0; f < 4; ++f) w.normals[f] = r * base_normals[f];
  return w;
}

#endif  // TRANSFORM_H
