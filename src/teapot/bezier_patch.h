#ifndef BEZIER_PATCH_H
#define BEZIER_PATCH_H

#include <common/vec_math.h>

// Cubic Bernstein basis, evaluated at t for all 4 control points at once.
inline void bernstein_basis(double t, double b[4]) {
  double mt = 1.0 - t;
  b[0] = mt * mt * mt;
  b[1] = 3.0 * t * mt * mt;
  b[2] = 3.0 * t * t * mt;
  b[3] = t * t * t;
}

// Derivative of the cubic Bernstein basis wrt t, for computing tangents.
inline void bernstein_basis_derivative(double t, double db[4]) {
  double mt = 1.0 - t;
  db[0] = -3.0 * mt * mt;
  db[1] = 3.0 * mt * mt - 6.0 * t * mt;
  db[2] = 6.0 * t * mt - 3.0 * t * t;
  db[3] = 3.0 * t * t;
}

// Position on a 4x4-control-point bicubic Bezier patch at parameters (u, v).
inline common::Vec3 evaluate_patch(
    const common::Vec3 (&cp)[4][4], double u, double v) {
  double bu[4], bv[4];
  bernstein_basis(u, bu);
  bernstein_basis(v, bv);

  common::Vec3 p{0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      p = p + cp[i][j] * (bu[i] * bv[j]);
    }
  }
  return p;
}

// Surface normal at (u, v) via the analytic partial-derivative tangents
// (cross of d/du and d/dv), rather than approximating it from neighboring
// tessellated vertices -- exact regardless of tessellation density.
inline common::Vec3 evaluate_patch_normal(
    const common::Vec3 (&cp)[4][4], double u, double v) {
  double bu[4], bv[4], dbu[4], dbv[4];
  bernstein_basis(u, bu);
  bernstein_basis(v, bv);
  bernstein_basis_derivative(u, dbu);
  bernstein_basis_derivative(v, dbv);

  common::Vec3 tangent_u{0, 0, 0};
  common::Vec3 tangent_v{0, 0, 0};
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      tangent_u = tangent_u + cp[i][j] * (dbu[i] * bv[j]);
      tangent_v = tangent_v + cp[i][j] * (bu[i] * dbv[j]);
    }
  }

  common::Vec3 n = common::cross(tangent_u, tangent_v);
  double len = common::length(n);
  // Degenerate at patch corners where control points coincide (e.g. the
  // lid's pole) and both tangents collapse; an arbitrary unit fallback
  // avoids a NaN rather than trying to be meaningful at a single point.
  if (len < 1e-12) return common::Vec3{0, 1, 0};
  return n * (1.0 / len);
}

#endif  // BEZIER_PATCH_H
