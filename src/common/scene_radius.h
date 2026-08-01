#ifndef COMMON_SCENE_RADIUS_H
#define COMMON_SCENE_RADIUS_H

#include <algorithm>

namespace common {

// Screen-space radius of a bounded object rendered onto a cols x rows*2
// "physics" grid (the cell-supersampling convention shared by sphere's and
// teapot's ASCII renderers), as a fraction of the smaller grid dimension.
// A single source of truth so a project's rendering scale and its
// bounce-physics collision radius never disagree.
inline double compute_scene_radius(int cols, int rows, double fraction) {
  return fraction * std::min(static_cast<double>(cols), rows * 2.0);
}

}  // namespace common

#endif  // COMMON_SCENE_RADIUS_H
