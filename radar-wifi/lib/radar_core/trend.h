#pragma once

#include <cstdint>

namespace radar {

enum class Trend { Stable, Approaching, Receding };

// Slope of the filtered RSSI over a sliding time window, with hysteresis so
// the display does not flicker between states.
class TrendTracker {
 public:
  static constexpr int kCapacity = 32;
  static constexpr uint32_t kWindowMs = 3000;
  static constexpr uint32_t kMinSpanMs = 1000;
  static constexpr int kMinSamples = 4;
  static constexpr float kEnter = 0.5f;  // dB/s
  static constexpr float kExit = 0.2f;   // dB/s

  void reset() {
    count_ = 0;
    head_ = 0;
    state_ = Trend::Stable;
  }

  Trend add(uint32_t ms, float rssi) {
    t_[head_] = ms;
    v_[head_] = rssi;
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity) count_++;

    float slope;
    if (!slopeDbPerSec(ms, slope)) {
      state_ = Trend::Stable;
      return state_;
    }
    switch (state_) {
      case Trend::Stable:
        if (slope > kEnter) state_ = Trend::Approaching;
        else if (slope < -kEnter) state_ = Trend::Receding;
        break;
      case Trend::Approaching:
        if (slope < kExit) state_ = slope < -kEnter ? Trend::Receding : Trend::Stable;
        break;
      case Trend::Receding:
        if (slope > -kExit) state_ = slope > kEnter ? Trend::Approaching : Trend::Stable;
        break;
    }
    return state_;
  }

  Trend state() const { return state_; }

 private:
  // Least-squares slope of the samples inside the window ending at `now`.
  bool slopeDbPerSec(uint32_t now, float& slope) const {
    int n = 0;
    uint32_t maxAge = 0;  // ages, not timestamps, so millis() wrap is harmless
    double st = 0, sv = 0, stt = 0, stv = 0;
    for (int i = 0; i < count_; i++) {
      const uint32_t age = now - t_[i];
      if (age > kWindowMs) continue;
      const double t = -static_cast<double>(age) / 1000.0;  // seconds, <= 0
      st += t;
      sv += v_[i];
      stt += t * t;
      stv += t * v_[i];
      if (age > maxAge) maxAge = age;
      n++;
    }
    if (n < kMinSamples || maxAge < kMinSpanMs) return false;
    const double den = n * stt - st * st;
    if (den <= 0) return false;
    slope = static_cast<float>((n * stv - st * sv) / den);
    return true;
  }

  uint32_t t_[kCapacity] = {};
  float v_[kCapacity] = {};
  int count_ = 0, head_ = 0;
  Trend state_ = Trend::Stable;
};

}  // namespace radar
