#pragma once

#include <array>
#include <string_view>

#include "sample_circle.h"

inline constexpr std::string_view kRamp = " .:-=+*#%@";

// Hand-approximated "ink" shape per character, in the same x∈[0,1],y∈[0,2]
// cell-fraction coordinates as the sample circles -- there's no
// font-rasterizer library in this repo, so real glyph bitmaps aren't
// available; these simple circle/rect predicates stand in for them, chosen
// so overall ink coverage increases monotonically down the ramp.
namespace glyph_ink {

inline bool InRect(double x, double y, double x0, double x1, double y0, double y1) {
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

inline bool InCircle(double x, double y, double cx, double cy, double r) {
    double dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= r * r;
}

inline bool InkDot(double x, double y) { return InCircle(x, y, 0.5, 1.72, 0.14); }

inline bool InkColon(double x, double y) {
    return InCircle(x, y, 0.5, 0.85, 0.13) || InCircle(x, y, 0.5, 1.55, 0.13);
}

inline bool InkDash(double x, double y) { return InRect(x, y, 0.15, 0.85, 0.90, 1.10); }

inline bool InkEquals(double x, double y) {
    return InRect(x, y, 0.15, 0.85, 0.65, 0.85) || InRect(x, y, 0.15, 0.85, 1.15, 1.35);
}

inline bool InkPlus(double x, double y) {
    return InRect(x, y, 0.10, 0.90, 0.90, 1.10) || InRect(x, y, 0.40, 0.60, 0.30, 1.70);
}

inline bool InkStar(double x, double y) { return InCircle(x, y, 0.5, 1.0, 0.40); }

inline bool InkHash(double x, double y) {
    return InRect(x, y, 0.24, 0.40, 0.05, 1.95) || InRect(x, y, 0.60, 0.76, 0.05, 1.95) ||
           InRect(x, y, 0.05, 0.95, 0.62, 0.80) || InRect(x, y, 0.05, 0.95, 1.20, 1.38);
}

inline bool InkPercent(double x, double y) {
    return InRect(x, y, 0.05, 0.95, 0.15, 1.85) && !InCircle(x, y, 0.28, 0.55, 0.16) &&
           !InCircle(x, y, 0.72, 1.45, 0.16);
}

inline bool InkAt(double, double) { return true; }

using InkFn = bool (*)(double, double);

// Index-aligned with kRamp; nullptr for space (all-zero shape vector).
inline constexpr std::array<InkFn, 10> kInkFns = {
    nullptr, InkDot, InkColon, InkDash, InkEquals, InkPlus, InkStar, InkHash, InkPercent, InkAt,
};

}  // namespace glyph_ink

// Fraction of the sample circle's area covered by the given ink predicate,
// via an NxN supersample grid over the circle's bounding box.
inline double InkFraction(glyph_ink::InkFn ink, double cx, double cy, double r) {
    if (ink == nullptr) return 0.0;

    constexpr int kN = 16;
    int total = 0, inside = 0;
    for (int i = 0; i < kN; ++i) {
        for (int j = 0; j < kN; ++j) {
            double sx = cx - r + (i + 0.5) * (2 * r / kN);
            double sy = cy - r + (j + 0.5) * (2 * r / kN);
            if ((sx - cx) * (sx - cx) + (sy - cy) * (sy - cy) > r * r) continue;
            ++total;
            if (ink(sx, sy)) ++inside;
        }
    }
    return total > 0 ? static_cast<double>(inside) / total : 0.0;
}

using ShapeVector = std::array<double, 6>;

// Each character's 6D shape vector, computed once at startup against the
// canonical sample-circle positions.
inline const std::array<ShapeVector, 10>& CharacterShapeVectors() {
    static const std::array<ShapeVector, 10> vectors = [] {
        std::array<ShapeVector, 10> v{};
        for (int c = 0; c < 10; ++c) {
            for (int k = 0; k < 6; ++k) {
                const auto& circle = kSampleCircles[k];
                v[c][k] = InkFraction(glyph_ink::kInkFns[c], circle.x, circle.y, circle.r);
            }
        }
        return v;
    }();
    return vectors;
}

// Euclidean nearest-neighbor match; a linear scan is plenty at N=10 (the
// blog's k-d tree is for much larger character alphabets).
inline char MatchChar(const ShapeVector& sample) {
    const auto& shapes = CharacterShapeVectors();
    int best = 0;
    double best_dist2 = -1.0;
    for (int c = 0; c < 10; ++c) {
        double sum = 0.0;
        for (int k = 0; k < 6; ++k) {
            double d = sample[k] - shapes[c][k];
            sum += d * d;
        }
        if (best_dist2 < 0.0 || sum < best_dist2) {
            best_dist2 = sum;
            best = c;
        }
    }
    return kRamp[best];
}
