#ifndef MESH_H
#define MESH_H

#include <array>

#include <common/vec_math.h>

// Regular tetrahedron centered at the origin (alternating corners of a cube).
constexpr common::Vec3 kLocalVertices[4] = {
    {1, 1, 1},
    {1, -1, -1},
    {-1, 1, -1},
    {-1, -1, 1},
};

// sqrt(3): exact distance of every vertex above from the origin.
constexpr double kCircumradius = 1.7320508075688772935;

// Faces as vertex-index triples, wound so common::cross(b-a, c-a) points
// outward (hand-verified: common::dot(that normal, any vertex on the face)
// > 0 for all four).
constexpr int kFaces[4][3] = {
    {1, 3, 2},  // opposite v0
    {0, 2, 3},  // opposite v1
    {0, 3, 1},  // opposite v2
    {0, 1, 2},  // opposite v3
};

// Local-space unit face normals, computed once since the mesh is rigid.
inline const std::array<common::Vec3, 4>& local_normals() {
  static const std::array<common::Vec3, 4> normals = [] {
    std::array<common::Vec3, 4> n;
    for (int f = 0; f < 4; ++f) {
      const common::Vec3& a = kLocalVertices[kFaces[f][0]];
      const common::Vec3& b = kLocalVertices[kFaces[f][1]];
      const common::Vec3& c = kLocalVertices[kFaces[f][2]];
      n[f] = common::normalize(common::cross(b - a, c - a));
    }
    return n;
  }();
  return normals;
}

#endif  // MESH_H
