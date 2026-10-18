#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace radar {

struct Network {
  uint8_t bssid[6];
  char ssid[33];  // 32 chars max + terminator; empty = hidden network
  int8_t rssi;
  uint8_t channel;
};

// Fixed-capacity list of access points, one entry per BSSID. No heap use.
class NetList {
 public:
  static constexpr int kCapacity = 24;

  void clear() { size_ = 0; }

  // Adds or merges a network. When full, it replaces the weakest entry only
  // if the new one is stronger.
  void add(const Network& n) {
    const int i = indexOf(n.bssid);
    if (i >= 0) {
      if (n.rssi > items_[i].rssi) items_[i] = n;
      return;
    }
    if (size_ < kCapacity) {
      items_[size_++] = n;
      return;
    }
    int weakest = 0;
    for (int j = 1; j < size_; j++) {
      if (items_[j].rssi < items_[weakest].rssi) weakest = j;
    }
    if (n.rssi > items_[weakest].rssi) items_[weakest] = n;
  }

  void sort() {
    std::stable_sort(items_, items_ + size_, [](const Network& a, const Network& b) {
      return a.rssi > b.rssi;
    });
  }

  int indexOf(const uint8_t bssid[6]) const {
    for (int i = 0; i < size_; i++) {
      if (std::memcmp(items_[i].bssid, bssid, 6) == 0) return i;
    }
    return -1;
  }

  int size() const { return size_; }
  const Network& at(int i) const { return items_[i]; }

 private:
  Network items_[kCapacity] = {};
  int size_ = 0;
};

}  // namespace radar
