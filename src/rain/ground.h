#ifndef GROUND_H
#define GROUND_H

#include <common/vec_math.h>

// May shrink during visual tuning: a square's diagonal corner extends past
// a circle of the same radius, so at kSceneBoundingRadius the corners can
// clip past the nominal on-screen extent camera.h sizes the scene to.
constexpr double kGroundHalfSize = 1.0;

constexpr double kGroundAlbedo = 0.9;

// Fixed in world space (unlike sphere's LightState, this never drifts --
// rain has no need for a moving highlight); transformed into camera space
// alongside the ground normal before shading, since the camera itself
// orbits here (see camera.h).
inline const common::Vec3 kLightDirWorld =
    common::normalize(common::Vec3{-0.4, 0.7, 0.5});

inline constexpr common::Vec3 kGroundVertices[4] = {
    {-kGroundHalfSize, 0.0, -kGroundHalfSize},
    {kGroundHalfSize, 0.0, -kGroundHalfSize},
    {kGroundHalfSize, 0.0, kGroundHalfSize},
    {-kGroundHalfSize, 0.0, kGroundHalfSize},
};

inline constexpr int kGroundTriangles[2][3] = {
    {0, 1, 2},
    {0, 2, 3},
};

inline constexpr common::Vec3 kGroundNormal{0.0, 1.0, 0.0};

#endif  // GROUND_H
