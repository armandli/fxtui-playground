#ifndef RAINDROP_H
#define RAINDROP_H

#include <cmath>
#include <random>
#include <vector>

#include <common/vec_math.h>
#include <ground.h>

constexpr double kSpawnHeight = 2.2;
constexpr double kFallSpeed = 0.026;
constexpr double kDriftMagnitude = 0.25;
constexpr double kMinStreakLength = 0.08;
constexpr double kMaxStreakLength = 0.22;
constexpr int kMaxActiveDrops = 800;

struct Drop {
  common::Vec3 head;      // leading (bottom) position, world space
  common::Vec3 velocity;  // world units/tick, fixed for the drop's lifetime
  double length;          // streak length, fixed for the drop's lifetime
};

// Random (x,z) within the ground's footprint, spawned kSpawnHeight above it
// (high enough to project off-screen at typical terminal sizes, so drops
// appear to fall into view rather than pop in mid-air). Velocity direction
// is randomized per drop -- mostly -Y with a small random X/Z drift, giving
// each drop its own slightly slanted fall rather than perfectly vertical.
inline Drop spawn_drop(std::mt19937& rng) {
  std::uniform_real_distribution<double> pos(
      -kGroundHalfSize, kGroundHalfSize);
  std::uniform_real_distribution<double> drift(
      -kDriftMagnitude, kDriftMagnitude);
  std::uniform_real_distribution<double> len(
      kMinStreakLength, kMaxStreakLength);

  common::Vec3 dir =
      common::normalize(common::Vec3{drift(rng), -1.0, drift(rng)});
  return Drop{
      common::Vec3{pos(rng), kSpawnHeight, pos(rng)},
      dir * kFallSpeed,
      len(rng),
  };
}

inline void advance_drops(std::vector<Drop>& drops) {
  for (auto& d : drops) d.head = d.head + d.velocity;
}

inline void remove_grounded(std::vector<Drop>& drops) {
  std::erase_if(drops, [](const Drop& d) { return d.head.y <= 0.0; });
}

// Fractional-rate spawning: guarantees floor(spawn_rate) new drops every
// tick, plus one more with probability equal to the fractional remainder,
// so the average spawn rate matches spawn_rate exactly even for
// non-integer values. Silently stops once kMaxActiveDrops is reached, as a
// safety ceiling against unbounded growth at high spawn rates.
inline void spawn_new_drops(
    std::vector<Drop>& drops, double spawn_rate, std::mt19937& rng) {
  if (static_cast<int>(drops.size()) >= kMaxActiveDrops) return;

  int guaranteed = static_cast<int>(std::floor(spawn_rate));
  double frac = spawn_rate - guaranteed;
  std::uniform_real_distribution<double> chance(0.0, 1.0);
  int to_spawn = guaranteed + (chance(rng) < frac ? 1 : 0);

  for (int i = 0;
       i < to_spawn and static_cast<int>(drops.size()) < kMaxActiveDrops;
       ++i) {
    drops.push_back(spawn_drop(rng));
  }
}

#endif  // RAINDROP_H
