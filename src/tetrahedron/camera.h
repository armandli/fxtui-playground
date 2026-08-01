#pragma once

#include <algorithm>

#include "mesh.h"
#include "vec_math.h"

struct Point {
    double x, y;
};

inline double NominalScale(int canvas_w, int canvas_h) {
    return 0.32 * std::min(canvas_w, canvas_h) / kCircumradius;
}

// Projects a world-space vertex orthographically, offset by the scene's
// on-screen center. Y is flipped: +Y is "up" in model space but canvas rows
// grow downward.
inline Point Project(const Vec3& world_vertex, double cx, double cy, double scale) {
    return {cx + scale * world_vertex.x, cy - scale * world_vertex.y};
}

// The camera sits on +Z looking toward -Z; with parallel orthographic view
// rays, "toward the camera" is the constant vector (0,0,1) everywhere, so
// visibility reduces to a sign check on the normal's Z component.
inline bool IsFrontFacing(const Vec3& world_normal) { return world_normal.z > 0.0; }
