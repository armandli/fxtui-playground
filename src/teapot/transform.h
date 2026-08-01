#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <cstddef>
#include <vector>

#include <common/rotation_state.h>
#include <common/vec_math.h>
#include <teapot_mesh.h>

struct WorldMesh {
  std::vector<common::Vec3> vertices;
  std::vector<common::Vec3> normals;
};

// Rebuilds from the cached local mesh fresh every call (rather than
// accumulating rotation by repeated multiplication) to avoid orthonormality
// drift over long runs, mirroring tetrahedron/transform.h. Index i in
// WorldMesh corresponds to index i in local_mesh() -- teapot_mesh.h's
// triangle indices stay valid against either.
inline WorldMesh transform(const common::Angles& a) {
  common::Mat3 r = common::build_rotation(a);
  const TeapotMesh& local = local_mesh();

  WorldMesh w;
  w.vertices.resize(local.vertices.size());
  w.normals.resize(local.normals.size());
  for (size_t i = 0; i < local.vertices.size(); ++i) {
    w.vertices[i] = r * local.vertices[i];
  }
  for (size_t i = 0; i < local.normals.size(); ++i) {
    w.normals[i] = r * local.normals[i];
  }
  return w;
}

#endif  // TRANSFORM_H
