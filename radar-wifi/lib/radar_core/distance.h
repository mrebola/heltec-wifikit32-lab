#pragma once

#include <algorithm>
#include <cmath>

namespace radar {

constexpr float kMinDistance = 0.1f;
constexpr float kMaxDistance = 999.0f;

// Log-distance path loss model.
struct DistanceModel {
  float rssiAt1m;  // RSSI measured at 1 m from the access point (dBm)
  float n;         // path loss exponent: ~2 free space, ~3 indoors
};

inline float estimateDistance(float rssi, const DistanceModel& m) {
  const float d = std::pow(10.0f, (m.rssiAt1m - rssi) / (10.0f * m.n));
  return std::min(std::max(d, kMinDistance), kMaxDistance);
}

struct DistanceRange {
  float min, est, max;
};

// Stronger signal = closer, so RSSI + sigma gives the lower bound.
inline DistanceRange distanceRange(float rssi, float sigma,
                                   const DistanceModel& m) {
  return {estimateDistance(rssi + sigma, m), estimateDistance(rssi, m),
          estimateDistance(rssi - sigma, m)};
}

}  // namespace radar
