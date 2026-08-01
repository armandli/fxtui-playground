#pragma once

#include <array>

#include "common/vec_math.h"

using namespace common;

// Regular tetrahedron centered at the origin (alternating corners of a cube).
constexpr Vec3 kLocalVertices[4] = {
    {1, 1, 1},
    {1, -1, -1},
    {-1, 1, -1},
    {-1, -1, 1},
};

// sqrt(3): exact distance of every vertex above from the origin.
constexpr double kCircumradius = 1.7320508075688772935;

// Faces as vertex-index triples, wound so Cross(b-a, c-a) points outward
// (hand-verified: Dot(that normal, any vertex on the face) > 0 for all four).
constexpr int kFaces[4][3] = {
    {1, 3, 2},  // opposite v0
    {0, 2, 3},  // opposite v1
    {0, 3, 1},  // opposite v2
    {0, 1, 2},  // opposite v3
};

// Local-space unit face normals, computed once since the mesh is rigid.
inline const std::array<Vec3, 4>& LocalNormals() {
    static const std::array<Vec3, 4> normals = [] {
        std::array<Vec3, 4> n;
        for (int f = 0; f < 4; ++f) {
            const Vec3& a = kLocalVertices[kFaces[f][0]];
            const Vec3& b = kLocalVertices[kFaces[f][1]];
            const Vec3& c = kLocalVertices[kFaces[f][2]];
            n[f] = Normalize(Cross(b - a, c - a));
        }
        return n;
    }();
    return normals;
}
