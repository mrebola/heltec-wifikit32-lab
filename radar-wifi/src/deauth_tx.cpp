#include "deauth_tx.h"

#include <esp_wifi.h>

#include "deauth.h"

// Raw-frame TX. The ESP32 driver does not touch the frame beyond appending
// the FCS, so the MAC addresses we write reach the air unchanged. Frames are
// sent as-is (`en_sys_seq = false`): we manage our own sequence numbers.

bool deauthTxSend(const uint8_t bssid[6], const uint8_t dest[6], uint16_t seq,
                  uint16_t reason) {
  uint8_t frame[radar::kDeauthLen];
  radar::buildDeauth(frame, bssid, dest, seq, reason);
  return esp_wifi_80211_tx(WIFI_IF_STA, frame, radar::kDeauthLen, false) == ESP_OK;
}