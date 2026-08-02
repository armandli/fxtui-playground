#ifndef SCENE_FIELD_H
#define SCENE_FIELD_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <camera.h>
#include <common/ascii_field.h>
#include <common/lighting.h>
#include <common/rasterizer2d.h>
#include <common/sample_circle.h>
#include <common/vec_math.h>
#include <ground.h>
#include <raindrop.h>

enum class Material : uint8_t { None, Ground, Rain };

struct SceneField {
  common::AsciiField brightness;
  std::vector<std::array<Material, 6>> material;
};

// Thickness (physics-space units, same space as common::kSampleCircles'
// ~0.30 sample-circle radius) of a raindrop's rendered streak. Kept
// smaller than a sample circle so a streak activates only 1-3 of a cell's
// 6 subsamples rather than flooding the whole cell -- it should read as a
// thin line, not a blob.
constexpr double kLineHalfWidth = 0.20;

struct CellBBox {
  int min_col, max_col, min_row, max_row;
};

// Converts a physics-space (px, py) bounding box into the range of grid
// cells it can touch, expanded by one cell and clamped to the grid --
// shared by the ground and raindrop rasterization passes below, both of
// which need to turn a projected shape's extent into a cell iteration
// range before testing individual common::kSampleCircles subsamples.
inline CellBBox compute_cell_bbox(
    double min_px, double max_px, double min_py, double max_py, int cols,
    int rows) {
  return {
      std::max(0, static_cast<int>(std::floor(min_px)) - 1),
      std::min(cols - 1, static_cast<int>(std::ceil(max_px)) + 1),
      std::max(0, static_cast<int>(std::floor(min_py / 2.0)) - 1),
      std::min(rows - 1, static_cast<int>(std::ceil(max_py / 2.0)) + 1),
  };
}

struct ClosestPoint {
  double t;     // 0 at a, 1 at b
  double dist;  // distance from (px, py) to the closest point on [a, b]
};

// Closest point on segment [a, b] to (px, py), via the standard clamped
// projection parameter t.
inline ClosestPoint closest_point_on_segment(
    const common::Point& a, const common::Point& b, double px, double py) {
  double abx = b.x - a.x, aby = b.y - a.y;
  double len2 = abx * abx + aby * aby;
  if (len2 < 1e-9) {
    double dx = px - a.x, dy = py - a.y;
    return {1.0, std::sqrt(dx * dx + dy * dy)};
  }
  double t = ((px - a.x) * abx + (py - a.y) * aby) / len2;
  t = std::clamp(t, 0.0, 1.0);
  double cpx = a.x + t * abx, cpy = a.y + t * aby;
  double dx = px - cpx, dy = py - cpy;
  return {t, std::sqrt(dx * dx + dy * dy)};
}

// Quadratic ramp toward the head (t=1), matching matrix's drop_color shape
// -- a sharper brightness peak near the leading edge than a linear fade.
inline double streak_brightness(double t) { return t * t; }

// Rasterizes the ground plane and every active raindrop's streak into a
// combined per-subsample brightness+material field, z-buffered together so
// draw order never matters (a raindrop passing behind/in front of the
// ground resolves correctly regardless of which is rasterized first).
inline SceneField rasterize_scene(
    const std::vector<Drop>& drops, double azimuth, int cols, int rows) {
  size_t cell_count = static_cast<size_t>(cols) * rows;
  SceneField field;
  field.brightness.assign(cell_count, std::array<double, 6>{});
  field.material.assign(cell_count, std::array<Material, 6>{});
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

  common::Mat3 cam = build_camera(azimuth);
  double scale = nominal_scale(cols, rows);
  double cx = cols / 2.0;
  double cy = rows;  // physics-space height is rows*2; center row = rows

  // Ground normal/light are transformed into camera space before shading:
  // common::shade's specular half-vector hard-codes (0,0,1) as "toward the
  // camera," which only holds once vectors are expressed in the camera's
  // own space -- unlike sphere/teapot's fixed camera, this one orbits, so
  // world space and camera space diverge here.
  common::Vec3 normal_cam = cam * kGroundNormal;
  common::Vec3 light_cam = cam * kLightDirWorld;
  double ground_brightness =
      common::shade(normal_cam, light_cam, kGroundAlbedo, 0.25, 0.15, 16.0);

  // --- Ground: 2 triangles, teapot-style barycentric bbox z-buffer pass ---
  common::Vec3 ground_cam[4];
  for (int i = 0; i < 4; ++i) ground_cam[i] = cam * kGroundVertices[i];

  if (is_front_facing(normal_cam)) {
    for (const auto& tri : kGroundTriangles) {
      const common::Vec3& v0 = ground_cam[tri[0]];
      const common::Vec3& v1 = ground_cam[tri[1]];
      const common::Vec3& v2 = ground_cam[tri[2]];

      common::Point p0 = project(v0, cx, cy, scale);
      common::Point p1 = project(v1, cx, cy, scale);
      common::Point p2 = project(v2, cx, cy, scale);

      double min_px = std::min({p0.x, p1.x, p2.x});
      double max_px = std::max({p0.x, p1.x, p2.x});
      double min_py = std::min({p0.y, p1.y, p2.y});
      double max_py = std::max({p0.y, p1.y, p2.y});

      CellBBox bbox =
          compute_cell_bbox(min_px, max_px, min_py, max_py, cols, rows);

      for (int row = bbox.min_row; row <= bbox.max_row; ++row) {
        for (int col = bbox.min_col; col <= bbox.max_col; ++col) {
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

            depth[idx][k] = z;
            field.brightness[idx][k] = ground_brightness;
            field.material[idx][k] = Material::Ground;
          }
        }
      }
    }
  }

  // --- Raindrops: 2D screen-space thick-line hit test per streak ---
  for (const auto& drop : drops) {
    common::Vec3 dir = common::normalize(drop.velocity);
    common::Vec3 tail = drop.head - dir * drop.length;

    common::Vec3 head_cam = cam * drop.head;
    common::Vec3 tail_cam = cam * tail;
    common::Point p_head = project(head_cam, cx, cy, scale);
    common::Point p_tail = project(tail_cam, cx, cy, scale);

    double min_px = std::min(p_head.x, p_tail.x) - kLineHalfWidth;
    double max_px = std::max(p_head.x, p_tail.x) + kLineHalfWidth;
    double min_py = std::min(p_head.y, p_tail.y) - kLineHalfWidth;
    double max_py = std::max(p_head.y, p_tail.y) + kLineHalfWidth;

    CellBBox bbox =
        compute_cell_bbox(min_px, max_px, min_py, max_py, cols, rows);

    for (int row = bbox.min_row; row <= bbox.max_row; ++row) {
      for (int col = bbox.min_col; col <= bbox.max_col; ++col) {
        size_t idx = static_cast<size_t>(row) * cols + col;
        for (int k = 0; k < 6; ++k) {
          const auto& circle = common::kSampleCircles[k];
          double px = col + circle.x;
          double py = row * 2.0 + circle.y;

          ClosestPoint hit =
              closest_point_on_segment(p_tail, p_head, px, py);
          if (hit.dist > kLineHalfWidth) continue;

          double z = tail_cam.z + hit.t * (head_cam.z - tail_cam.z);
          if (z <= depth[idx][k]) continue;

          depth[idx][k] = z;
          field.brightness[idx][k] = streak_brightness(hit.t);
          field.material[idx][k] = Material::Rain;
        }
      }
    }
  }

  return field;
}

#endif  // SCENE_FIELD_H
