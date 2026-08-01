#pragma once

#include <algorithm>
#include <array>
#include <vector>

#include "common/bouncing_scene.h"
#include "glyphs.h"
#include "sample_circle.h"
#include "sphere_field.h"
#include "transform.h"

// Smaller than a comparable prior app's 0.32 fraction so the sphere
// doesn't fill the frame and bounce motion stays clearly visible.
constexpr double kRadiusFraction = 0.28;

// Single source of truth for the sphere's screen-space radius, used both
// by RenderFrame below and by the bounce-physics update in main.cpp so
// they never disagree on the play field.
inline double ComputeRadius(int cols, int rows) {
    return kRadiusFraction * std::min(static_cast<double>(cols), rows * 2.0);
}

// Two-pass per-frame render: pass 1 computes every cell's raw 6D sample
// vector from the sphere/lighting field; pass 2 applies contrast
// enhancement (which needs neighboring cells' pass-1 vectors) and matches
// each cell against the nearest character.
inline std::vector<char> RenderFrame(const Angles& angles, const Scene& scene, const LightState& light, int cols,
                                      int rows) {
    double radius = ComputeRadius(cols, rows);
    Mat3 body_rotation = BuildRotation(angles);

    std::vector<std::array<double, 6>> field(static_cast<size_t>(cols) * rows);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto& v = field[row * cols + col];
            for (int k = 0; k < 6; ++k) {
                const auto& circle = kSampleCircles[k];
                double px = col + circle.x;
                double py = row * 2.0 + circle.y;
                double dx = px - scene.cx;
                double dy = py - scene.cy;
                auto result = ShadeSample(dx, dy, radius, body_rotation, light.dir);
                v[k] = result.value_or(0.0);
            }
        }
    }

    for (auto& v : field) ApplyGlobalContrast(v);

    std::vector<char> output(static_cast<size_t>(cols) * rows);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto v = field[row * cols + col];  // copy: folding must not mutate a neighbor's stored value
            if (col > 0) v[0] = DirectionalFold(v[0], field[row * cols + (col - 1)][5]);
            if (col < cols - 1) v[5] = DirectionalFold(v[5], field[row * cols + (col + 1)][0]);
            output[row * cols + col] = MatchChar(v);
        }
    }

    return output;
}
