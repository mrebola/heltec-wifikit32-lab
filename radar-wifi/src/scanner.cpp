#include "scanner.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include <cstring>

// Completion is detected with the SCAN_DONE event, not WiFi.scanComplete().
// The core gives an async scan only max_ms_per_chan * 20 before calling it
// failed; a full sweep can take longer, and after such a false timeout every
// later scan picks up the previous one's late SCAN_DONE and reports 0 networks.

namespace {

constexpr uint32_t kMsPerChannel = 120;
constexpr uint32_t kStopAfterMs = 8000;     // watchdog: abort a stuck scan
constexpr uint32_t kRestartAfterMs = 10000;  // and restart WiFi if still silent

bool running = false;
uint32_t startedAt = 0;
bool stopRequested = false;
volatile bool doneEvent = false;  // set from the WiFi event task

// Last resort when SCAN_DONE never arrives. Restarting the driver drops any
// late event, and scanComplete() clears the core's "scanning" flag, which
// would otherwise make every later scanNetworks() a silent no-op.
void restartWifi() {
  esp_wifi_scan_stop();
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP_STA);  // AP block must exist for raw-frame TX (deauth)
  WiFi.scanComplete();
  WiFi.scanDelete();
  doneEvent = false;
}

}  // namespace

void scannerBegin() {
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) { doneEvent = true; },
               ARDUINO_EVENT_WIFI_SCAN_DONE);
  WiFi.persistent(false);  // never write WiFi config to flash
  // APSTA + a configured hidden softAP: raw-frame TX needs an AP interface
  // that is started and configured, otherwise esp_wifi_80211_tx accepts the
  // frame but it never reaches the air (verified on hardware). Same setup as
  // ESP32 Marauder's attack mode: empty hidden SSID, beacon every 10 s.
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();
  wifi_config_t apConf = {};
  apConf.ap.ssid_hidden = 1;
  apConf.ap.beacon_interval = 10000;
  apConf.ap.ssid_len = 0;
  apConf.ap.channel = 1;
  apConf.ap.authmode = WIFI_AUTH_OPEN;
  apConf.ap.max_connection = 1;
  esp_wifi_set_config(WIFI_IF_AP, &apConf);
}

bool scannerBusy() { return running; }

void scannerStart() {
  if (running) return;
  doneEvent = false;
  if (WiFi.scanNetworks(true, true, false, kMsPerChannel) != WIFI_SCAN_RUNNING) return;
  running = true;
  startedAt = millis();
  stopRequested = false;
}

ScanResult scannerPoll(radar::NetList& out) {
  if (!running) return ScanResult::Failed;

  if (!doneEvent) {
    const uint32_t elapsed = millis() - startedAt;
    if (elapsed >= kRestartAfterMs) {
      running = false;
      restartWifi();
      return ScanResult::Failed;
    }
    if (elapsed >= kStopAfterMs && !stopRequested) {
      esp_wifi_scan_stop();  // SCAN_DONE follows
      stopRequested = true;
    }
    return ScanResult::Running;
  }

  running = false;
  const int16_t n = WiFi.scanComplete();
  if (n < 0 || stopRequested) {
    WiFi.scanDelete();
    return ScanResult::Failed;
  }

  out.clear();
  for (int16_t i = 0; i < n; i++) {
    radar::Network net{};
    std::memcpy(net.bssid, WiFi.BSSID(i), sizeof(net.bssid));
    std::strncpy(net.ssid, WiFi.SSID(i).c_str(), sizeof(net.ssid) - 1);
    net.rssi = static_cast<int8_t>(WiFi.RSSI(i));
    net.channel = static_cast<uint8_t>(WiFi.channel(i));
    out.add(net);
  }
  out.sort();
  WiFi.scanDelete();
  return ScanResult::Done;
}
