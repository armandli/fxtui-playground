#pragma once

#include <array>

#include "common/angles.h"
#include "common/vec_math.h"
#include "mesh.h"

using namespace common;

// Slow, independent-rate rotation on all three axes so the tetrahedron reads
// as genuinely tumbling rather than spinning flat around one edge-on axis.
constexpr double kStepX = 0.9 * kDegToRad;
constexpr double kStepY = 0.6 * kDegToRad;
constexpr double kStepZ = 0.4 * kDegToRad;

inline void Advance(Angles& a) {
    a.x = WrapAngle(a.x + kStepX);
    a.y = WrapAngle(a.y + kStepY);
    a.z = WrapAngle(a.z + kStepZ);
}

struct WorldMesh {
    std::array<Vec3, 4> vertices;
    std::array<Vec3, 4> normals;
};

// Rebuilds the rotation matrix fresh from the persistent angles every call
// (rather than accumulating it by repeated multiplication) to avoid
// orthonormality drift over long runs.
inline WorldMesh Transform(const Angles& a) {
    Mat3 r = RotationZ(a.z) * RotationY(a.y) * RotationX(a.x);
    const auto& local_normals = LocalNormals();

    WorldMesh w;
    for (int i = 0; i < 4; ++i) w.vertices[i] = r * kLocalVertices[i];
    for (int f = 0; f < 4; ++f) w.normals[f] = r * local_normals[f];
    return w;
}
