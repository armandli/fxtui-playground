#pragma once

#include <ftxui/dom/canvas.hpp>

#include "camera.h"
#include "mesh.h"
#include "rasterizer.h"
#include "scene.h"
#include "shading.h"
#include "transform.h"

// Sequences the pipeline for one frame: model transform -> per face
// (backface cull -> project -> shade -> rasterize) -> canvas. Visible faces
// never overlap in screen space for a convex solid under orthographic
// projection, so draw order among them doesn't affect correctness.
inline void RenderFrame(const Angles& angles, const Scene& scene, ftxui::Canvas& canvas) {
    double scale = NominalScale(canvas.width(), canvas.height());
    WorldMesh world = Transform(angles);

    for (int f = 0; f < 4; ++f) {
        if (!IsFrontFacing(world.normals[f])) continue;

        Point p0 = Project(world.vertices[kFaces[f][0]], scene.cx, scene.cy, scale);
        Point p1 = Project(world.vertices[kFaces[f][1]], scene.cx, scene.cy, scale);
        Point p2 = Project(world.vertices[kFaces[f][2]], scene.cx, scene.cy, scale);

        FillTriangle(canvas, p0, p1, p2, ShadeFace(world.normals[f]));
    }
}
