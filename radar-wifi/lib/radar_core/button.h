#pragma once

#include <cstdint>

namespace radar {

enum class Gesture { None, Short, Long, VeryLong };

// Single-button input. Every gesture fires on release; what it is depends on
// how long the button was held. A quick tap is Short. Holding on cycles
// Long -> VeryLong -> Short -> Long ... one step at a time, so the user can
// wait until the screen shows the action they want and let go.
// Time is passed in so it can be tested without hardware.
class Button {
 public:
  static constexpr uint32_t kDebounceMs = 25;
  static constexpr uint32_t kLongMs = 600;    // end of the initial Short phase
  static constexpr uint32_t kStepMs = 1200;   // time on each action after that
  static constexpr uint32_t kVeryLongMs = kLongMs + kStepMs;

  // The gesture a press of `heldMs` produces.
  static Gesture gestureAt(uint32_t heldMs) {
    if (heldMs < kLongMs) return Gesture::Short;
    switch ((heldMs - kLongMs) / kStepMs % 3) {
      case 0: return Gesture::Long;
      case 1: return Gesture::VeryLong;
      default: return Gesture::Short;
    }
  }

  // Time until gestureAt() changes.
  static uint32_t msToNext(uint32_t heldMs) {
    if (heldMs < kLongMs) return kLongMs - heldMs;
    return kStepMs - (heldMs - kLongMs) % kStepMs;
  }

  Gesture update(bool pressed, uint32_t now) {
    if (pressed != raw_) {
      raw_ = pressed;
      rawSince_ = now;
    }
    if (raw_ != stable_ && now - rawSince_ >= kDebounceMs) {
      stable_ = raw_;
      if (stable_) pressedAt_ = rawSince_;
      else return gestureAt(rawSince_ - pressedAt_);
    }
    return Gesture::None;
  }

  bool held() const { return stable_; }
  uint32_t heldMs(uint32_t now) const { return held() ? now - pressedAt_ : 0; }

  // The gesture that releasing at `now` would produce (None if not held).
  Gesture pending(uint32_t now) const {
    return held() ? gestureAt(heldMs(now)) : Gesture::None;
  }

 private:
  bool raw_ = false, stable_ = false;
  uint32_t rawSince_ = 0, pressedAt_ = 0;
};

}  // namespace radar
