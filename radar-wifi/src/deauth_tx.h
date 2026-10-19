#pragma once

#include <cstdint>

// Transmits forged 802.11 management frames (lab use: your own or authorized
// networks only). Requires the sniffer to be active on the target's channel:
// it reuses that mode so no extra radio setup is needed.

// Sends one deauthentication frame spoofing the AP `bssid` to `dest`
// (broadcast or one client). `seq` must advance between sends. Returns
// ESP_OK (0) when the driver accepted it, or the ESP-IDF error code.
int deauthTxSend(const uint8_t bssid[6], const uint8_t dest[6], uint16_t seq,
                 uint16_t reason);