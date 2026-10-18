#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace radar {

// What a raw 802.11 frame tells us about one access point.
struct FrameInfo {
  bool fromAp;        // transmitted by the AP: its RSSI measures our distance
  bool hasClient;     // involves a unicast station associated with the AP
  uint8_t client[6];
};

namespace detail {
inline bool same(const uint8_t* a, const uint8_t* b) { return std::memcmp(a, b, 6) == 0; }
inline bool unicast(const uint8_t* mac) { return (mac[0] & 0x01) == 0; }
}  // namespace detail

// Reads the header of a management or data frame. Returns false when the
// frame does not belong to `bssid` or cannot be interpreted.
inline bool parseFrame(const uint8_t* f, size_t len, const uint8_t bssid[6],
                       FrameInfo& out) {
  if (len < 24) return false;
  const int type = (f[0] >> 2) & 0x3;  // 0 mgmt, 1 control, 2 data
  const bool toDs = f[1] & 0x01, fromDs = f[1] & 0x02;
  const uint8_t* a1 = f + 4;
  const uint8_t* a2 = f + 10;
  const uint8_t* a3 = f + 16;

  const uint8_t* station = nullptr;
  if (type == 0) {
    if (!detail::same(a3, bssid)) return false;
    out.fromAp = detail::same(a2, bssid);
    station = out.fromAp ? a1 : a2;
  } else if (type == 2 && toDs && !fromDs) {
    if (!detail::same(a1, bssid)) return false;
    out.fromAp = false;
    station = a2;
  } else if (type == 2 && fromDs && !toDs) {
    if (!detail::same(a2, bssid)) return false;
    out.fromAp = true;
    station = a1;
  } else {
    return false;
  }

  out.hasClient = detail::unicast(station) && !detail::same(station, bssid);
  // A beacon or probe response sent by the AP says nothing about clients.
  if (type == 0 && out.fromAp) out.hasClient = false;
  if (out.hasClient) std::memcpy(out.client, station, 6);
  return true;
}

// Stations seen talking to the AP, each with the last time it was heard.
class ClientTable {
 public:
  static constexpr int kCapacity = 48;

  void clear() { size_ = 0; }

  void seen(const uint8_t mac[6], uint32_t now) {
    int oldest = 0;
    for (int i = 0; i < size_; i++) {
      if (detail::same(mac_[i], mac)) {
        last_[i] = now;
        return;
      }
      if (now - last_[i] > now - last_[oldest]) oldest = i;
    }
    const int slot = size_ < kCapacity ? size_++ : oldest;
    std::memcpy(mac_[slot], mac, 6);
    last_[slot] = now;
  }

  int count(uint32_t now, uint32_t windowMs) const {
    int n = 0;
    for (int i = 0; i < size_; i++) {
      if (now - last_[i] <= windowMs) n++;
    }
    return n;
  }

  bool contains(const uint8_t mac[6]) const {
    for (int i = 0; i < size_; i++) {
      if (detail::same(mac_[i], mac)) return true;
    }
    return false;
  }

 private:
  uint8_t mac_[kCapacity][6] = {};
  uint32_t last_[kCapacity] = {};
  int size_ = 0;
};

}  // namespace radar
