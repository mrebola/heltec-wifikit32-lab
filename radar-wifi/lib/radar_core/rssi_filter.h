#pragma once

#include <algorithm>
#include <cmath>

namespace radar {

// Median of the last few readings (drops spikes) followed by a 1D Kalman
// filter. Filtering is done on RSSI, not distance, because distance is
// exponential in RSSI and would amplify noise.
class RssiFilter {
 public:
  static constexpr int kWindow = 5;
  static constexpr float kMinSigma = 3.0f;

  explicit RssiFilter(float q = 0.8f, float r = 6.0f) : q_(q), r_(r) {}

  void reset() {
    count_ = 0;
    head_ = 0;
    samples_ = 0;
    ready_ = false;
  }

  float update(float rssi) {
    samples_++;
    window_[head_] = rssi;
    head_ = (head_ + 1) % kWindow;
    if (count_ < kWindow) count_++;
    const float z = median();

    if (!ready_) {
      x_ = z;
      p_ = r_;
      ready_ = true;
      return x_;
    }
    p_ += q_;
    const float k = p_ / (p_ + r_);
    x_ += k * (z - x_);
    p_ *= 1.0f - k;
    return x_;
  }

  bool ready() const { return ready_; }
  // Readings since the last reset.
  int samples() const { return samples_; }
  float value() const { return x_; }
  float sigma() const {
    const float s = std::sqrt(p_);
    return s > kMinSigma ? s : kMinSigma;
  }

 private:
  float median() const {
    float sorted[kWindow];
    std::copy(window_, window_ + count_, sorted);
    std::sort(sorted, sorted + count_);
    return count_ % 2 ? sorted[count_ / 2]
                      : (sorted[count_ / 2 - 1] + sorted[count_ / 2]) / 2.0f;
  }

  float q_, r_;
  float window_[kWindow] = {};
  int count_ = 0, head_ = 0, samples_ = 0;
  float x_ = 0.0f, p_ = 0.0f;
  bool ready_ = false;
};

}  // namespace radar
