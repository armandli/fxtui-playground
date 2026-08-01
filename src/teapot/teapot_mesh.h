#ifndef TEAPOT_MESH_H
#define TEAPOT_MESH_H

#include <array>
#include <vector>

#include <bezier_patch.h>
#include <common/vec_math.h>
#include <teapot_control_points.h>

// Samples per patch side; (N-1)^2 quads = 2*(N-1)^2 triangles per patch, so
// kNumPatches * 2*(N-1)^2 triangles total (98 tris/patch * 28 = 2744 at N=8).
constexpr int kPatchSubdivisions = 8;

struct TeapotMesh {
  std::vector<common::Vec3> vertices;
  std::vector<common::Vec3> normals;
  std::vector<std::array<int, 3>> triangles;
};

// Tessellates every control patch into a fixed-resolution triangle grid in
// local (origin-centered) object space, with exact analytic per-vertex
// normals from the Bezier surface's own tangents (no face-normal averaging
// needed). Patches are tessellated independently -- Newell's control points
// already agree exactly along shared patch edges, so no explicit vertex
// welding is needed for a seamless-looking result even though each patch
// contributes its own disjoint vertices.
//
// Computed once via this function-local static (mirroring
// common/glyphs.h's character_shape_vectors() caching pattern), since the
// mesh is rigid -- only its rotation changes frame to frame, never its
// shape.
inline const TeapotMesh& local_mesh() {
  static const TeapotMesh mesh = [] {
    TeapotMesh m;
    constexpr int N = kPatchSubdivisions;
    m.vertices.reserve(static_cast<size_t>(kNumPatches) * N * N);
    m.normals.reserve(m.vertices.capacity());
    m.triangles.reserve(
        static_cast<size_t>(kNumPatches) * (N - 1) * (N - 1) * 2);

    for (int p = 0; p < kNumPatches; ++p) {
      for (int i = 0; i < N; ++i) {
        double u = static_cast<double>(i) / (N - 1);
        for (int j = 0; j < N; ++j) {
          double v = static_cast<double>(j) / (N - 1);
          m.vertices.push_back(evaluate_patch(kControlPoints[p], u, v));
          m.normals.push_back(evaluate_patch_normal(kControlPoints[p], u, v));
        }
      }
    }

    // The cross(tangent_u, tangent_v) convention in evaluate_patch_normal
    // could point either outward or inward depending on the control
    // points' winding, uniformly across every patch. Rather than hand-guess
    // the sign, orient by the geometric fact that most surface area of a
    // roughly star-shaped-from-center blob (the origin-centered teapot)
    // faces away from its own center: flip every normal if the average
    // dot(normal, vertex) across all vertices comes out negative.
    double outward_sum = 0.0;
    for (size_t i = 0; i < m.vertices.size(); ++i) {
      outward_sum += common::dot(m.normals[i], m.vertices[i]);
    }
    if (outward_sum < 0.0) {
      for (auto& n : m.normals) n = n * -1.0;
    }

    for (int p = 0; p < kNumPatches; ++p) {
      int base = p * N * N;
      for (int i = 0; i < N - 1; ++i) {
        for (int j = 0; j < N - 1; ++j) {
          int i00 = base + i * N + j;
          int i01 = base + i * N + (j + 1);
          int i10 = base + (i + 1) * N + j;
          int i11 = base + (i + 1) * N + (j + 1);

          // Wind each triangle to agree with the now-consistently-oriented
          // vertex normals, rather than assuming a fixed index order is
          // correct -- self-correcting regardless of the patch's own
          // parametrization direction.
          auto orient = [&](int a, int b, int c) {
            common::Vec3 face_n = common::cross(
                m.vertices[b] - m.vertices[a], m.vertices[c] - m.vertices[a]);
            common::Vec3 avg_n = m.normals[a] + m.normals[b] + m.normals[c];
            if (common::dot(face_n, avg_n) < 0.0) {
              m.triangles.push_back({a, c, b});
            } else {
              m.triangles.push_back({a, b, c});
            }
          };
          orient(i00, i10, i11);
          orient(i00, i11, i01);
        }
      }
    }

    return m;
  }();
  return mesh;
}

#endif  // TEAPOT_MESH_H
