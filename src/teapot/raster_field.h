#ifndef RASTER_FIELD_H
#define RASTER_FIELD_H

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

#include <camera.h>
#include <common/ascii_field.h>
#include <common/light_state.h>
#include <common/lighting.h>
#include <common/rasterizer2d.h>
#include <common/sample_circle.h>
#include <common/vec_math.h>
#include <teapot_mesh.h>
#include <transform.h>

// Rasterizes a rotated teapot mesh into a per-subsample brightness field, at
// the resolution and sample positions common::ascii_from_field expects.
// Unlike the convex tetrahedron (where backface culling alone suffices,
// since visible faces never overlap in screen space), the teapot
// self-occludes -- the handle loops behind the body, the spout crosses in
// front of it -- so each subsample tracks the nearest front-facing triangle
// it has seen so far (a z-buffer) rather than just the last one drawn.
inline common::AsciiField raster_field(
    const WorldMesh& world,
    const common::LightState& light,
    double cx,
    double cy,
    double scale,
    int cols,
    int rows) {
  size_t cell_count = static_cast<size_t>(cols) * rows;
  common::AsciiField field(cell_count, std::array<double, 6>{});
  std::vector<std::array<double, 6>> depth(
      cell_count,
      std::array<double, 6>{
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(),
      });

  // Triangle vertex indices are shared between local_mesh() and world (a
  // 1:1 rotated copy of it), so the index list is reused here unchanged.
  const TeapotMesh& mesh = local_mesh();

  for (const auto& tri : mesh.triangles) {
    const common::Vec3& v0 = world.vertices[tri[0]];
    const common::Vec3& v1 = world.vertices[tri[1]];
    const common::Vec3& v2 = world.vertices[tri[2]];
    const common::Vec3& n0 = world.normals[tri[0]];
    const common::Vec3& n1 = world.normals[tri[1]];
    const common::Vec3& n2 = world.normals[tri[2]];

    // Cheap pre-cull on the summed vertex normal, ahead of the definitive
    // per-subsample depth test below.
    if (not is_front_facing(n0 + n1 + n2)) continue;

    common::Point p0 = project(v0, cx, cy, scale);
    common::Point p1 = project(v1, cx, cy, scale);
    common::Point p2 = project(v2, cx, cy, scale);

    double min_px = std::min({p0.x, p1.x, p2.x});
    double max_px = std::max({p0.x, p1.x, p2.x});
    double min_py = std::min({p0.y, p1.y, p2.y});
    double max_py = std::max({p0.y, p1.y, p2.y});

    int min_col = std::max(0, static_cast<int>(std::floor(min_px)) - 1);
    int max_col = std::min(cols - 1, static_cast<int>(std::ceil(max_px)) + 1);
    int min_row = std::max(0, static_cast<int>(std::floor(min_py / 2.0)) - 1);
    int max_row = std::min(
        rows - 1,
        static_cast<int>(std::ceil(max_py / 2.0)) + 1);

    for (int row = min_row; row <= max_row; ++row) {
      for (int col = min_col; col <= max_col; ++col) {
        size_t idx = static_cast<size_t>(row) * cols + col;
        for (int k = 0; k < 6; ++k) {
          const auto& circle = common::kSampleCircles[k];
          double px = col + circle.x;
          double py = row * 2.0 + circle.y;

          auto bary = common::barycentric(p0, p1, p2, px, py);
          if (not bary) continue;
          double wa = bary->x, wb = bary->y, wc = bary->z;

          double z = wa * v0.z + wb * v1.z + wc * v2.z;
          if (z <= depth[idx][k]) continue;

          common::Vec3 normal = common::normalize(n0 * wa + n1 * wb + n2 * wc);
          depth[idx][k] = z;
          field[idx][k] = common::shade(normal, light.dir, 1.0);
        }
      }
    }
  }

  return field;
}

#endif  // RASTER_FIELD_H
