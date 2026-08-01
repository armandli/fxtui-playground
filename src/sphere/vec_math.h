#pragma once

#include <cmath>

struct Vec3 {
    double x, y, z;
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(const Vec3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }

inline double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline double Length(const Vec3& a) { return std::sqrt(Dot(a, a)); }

inline Vec3 Normalize(const Vec3& a) {
    double len = Length(a);
    return {a.x / len, a.y / len, a.z / len};
}

// Row-major 3x3 matrix.
struct Mat3 {
    double m[3][3];
};

inline Mat3 RotationX(double t) {
    double c = std::cos(t), s = std::sin(t);
    return {{{1, 0, 0}, {0, c, -s}, {0, s, c}}};
}

inline Mat3 RotationY(double t) {
    double c = std::cos(t), s = std::sin(t);
    return {{{c, 0, s}, {0, 1, 0}, {-s, 0, c}}};
}

inline Mat3 RotationZ(double t) {
    double c = std::cos(t), s = std::sin(t);
    return {{{c, -s, 0}, {s, c, 0}, {0, 0, 1}}};
}

inline Mat3 operator*(const Mat3& a, const Mat3& b) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            for (int k = 0; k < 3; ++k) {
                r.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return r;
}

inline Vec3 operator*(const Mat3& a, const Vec3& v) {
    return {
        a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
        a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
        a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z,
    };
}

// Transpose == inverse for an orthonormal (pure rotation) matrix; used to
// un-spin a world-space point back into the body's own rotation frame.
inline Mat3 Transpose(const Mat3& a) {
    Mat3 r{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.m[i][j] = a.m[j][i];
        }
    }
    return r;
}
