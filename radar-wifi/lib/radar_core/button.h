#pragma once

#include <cstdint>

namespace radar {

enum class Gesture { None, Short, Long, VeryLong };

// Single-button gesture detector. Time is passed in so it can be tested
// without hardware. Short and Long fire on release; VeryLong fires as soon
// as the threshold is reached so the user knows when to let go.
class Button {
 public:
  static constexpr uint32_t kDebounceMs = 25;
  static constexpr uint32_t kLongMs = 600;
  static constexpr uint32_t kVeryLongMs = 2000;

  Gesture update(bool pressed, uint32_t now) {
    if (pressed != raw_) {
      raw_ = pressed;
      rawSince_ = now;
    }
    if (raw_ != stable_ && now - rawSince_ >= kDebounceMs) {
      stable_ = raw_;
      if (stable_) {
        pressedAt_ = rawSince_;
        consumed_ = false;
      } else if (!consumed_) {
        return rawSince_ - pressedAt_ < kLongMs ? Gesture::Short : Gesture::Long;
      }
    }
    if (stable_ && !consumed_ && now - pressedAt_ >= kVeryLongMs) {
      consumed_ = true;
      return Gesture::VeryLong;
    }
    return Gesture::None;
  }

  bool held() const { return stable_ && !consumed_; }
  uint32_t heldMs(uint32_t now) const { return held() ? now - pressedAt_ : 0; }

 private:
  bool raw_ = false, stable_ = false, consumed_ = false;
  uint32_t rawSince_ = 0, pressedAt_ = 0;
};

}  // namespace radar
