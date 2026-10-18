// WiFi radar for the Heltec WiFi Kit 32.
// List access points by signal, lock onto one, and walk towards it.
// One button (PRG): short = next, long = OK, very long = back.

#include <Arduino.h>
#include <Preferences.h>

#include "button.h"
#include "distance.h"
#include "nav.h"
#include "net_list.h"
#include "rssi_filter.h"
#include "scanner.h"
#include "sniffer.h"
#include "trend.h"
#include "ui.h"

using namespace radar;

namespace {

constexpr int kButtonPin = 0;  // PRG
constexpr uint32_t kSplashMs = 2000;
constexpr uint32_t kListRescanMs = 3000;
constexpr uint32_t kSampleMs = 125;     // one filtered RSSI reading per period
constexpr uint32_t kLostAfterMs = 5000;
constexpr uint32_t kClientWindowMs = 60000;
constexpr uint32_t kFrameMs = 50;
constexpr uint32_t kToastMs = 1500;
constexpr int kMinCalibrationSamples = 8;  // ~1 s of readings
constexpr float kDefaultRssiAt1m = -40.0f;
constexpr float kIndoorN = 3.0f;
constexpr float kOutdoorN = 2.2f;

Button button;
Nav nav;
NetList nets;     // what the list screen shows
NetList results;  // scratch space for the last scan
RssiFilter filter;
TrendTracker trend;
Preferences prefs;

Network target{};
float rssiAt1m = kDefaultRssiAt1m;
bool outdoor = false;
uint32_t lastFullScanAt = 0;
bool rescanNow = true;
uint32_t lastSampleAt = 0;
uint32_t lastSeenAt = 0;
bool everSeen = false;   // got a reading since tracking started
bool listenNext = false;  // start the sniffer as soon as the radio is free
const char* toast = nullptr;
uint32_t toastUntil = 0;
uint32_t lastFrameAt = 0;

DistanceModel model() { return {rssiAt1m, outdoor ? kOutdoorN : kIndoorN}; }

bool tracking() { return nav.screen != Screen::List; }
bool signalOk(uint32_t now) { return everSeen && now - lastSeenAt <= kLostAfterMs; }

void showToast(const char* msg, uint32_t now) {
  toast = msg;
  toastUntil = now + kToastMs;
}

void saveSettings() {
  prefs.putFloat("a1m", rssiAt1m);
  prefs.putBool("out", outdoor);
}

void addReading(float rssi, uint32_t now) {
  if (!signalOk(now)) {  // first reading, or back after losing it
    filter.reset();
    trend.reset();
  }
  trend.add(now, filter.update(rssi));
  lastSeenAt = now;
  everSeen = true;
}

// Replaces the list, keeping the cursor on the same access point.
void updateList() {
  int keep = nav.cursor;
  if (nav.cursor < nets.size()) {
    const int i = results.indexOf(nets.at(nav.cursor).bssid);
    if (i >= 0) keep = i;
  }
  nets = results;
  nav.setCursor(keep, nets.size());
}

void handleAction(Action a, uint32_t now) {
  switch (a) {
    case Action::Rescan:
      rescanNow = true;
      showToast("Escaneando...", now);
      break;
    case Action::StartTrack:
      target = nets.at(nav.cursor);
      filter.reset();
      trend.reset();
      everSeen = false;
      lastSeenAt = now;
      listenNext = true;  // starts once any running scan has finished
      break;
    case Action::StopTrack:
      snifferStop();
      listenNext = false;
      rescanNow = true;
      break;
    case Action::Calibrate:
      if (signalOk(now) && filter.samples() >= kMinCalibrationSamples) {
        rssiAt1m = filter.value();
        saveSettings();
        showToast("Calibrado a 1 m", now);
      } else {
        showToast("Espera la señal", now);
      }
      break;
    case Action::ToggleEnv:
      outdoor = !outdoor;
      saveSettings();
      break;
    case Action::None:
      break;
  }
}

// The radio does one thing at a time: a full scan (list, or looking for a
// lost target on other channels) or listening to the target's channel.
void updateRadio(uint32_t now) {
  if (scannerBusy()) {
    if (scannerPoll(results) != ScanResult::Done) return;
    updateList();
    Serial.printf("scan: %d redes\n", nets.size());
    if (tracking()) {
      const int i = results.indexOf(target.bssid);
      if (i >= 0) {
        target.channel = results.at(i).channel;  // the AP may have moved
        listenNext = true;
      }
    }
    return;
  }

  if (!tracking()) {
    if (rescanNow || now - lastFullScanAt >= kListRescanMs) {
      scannerStart();
      rescanNow = false;
      lastFullScanAt = now;
    }
    return;
  }

  if (listenNext) {
    snifferStart(target, everSeen);
    listenNext = false;
    lastSeenAt = now;
    return;
  }
  if (!snifferActive()) {
    scannerStart();
    return;
  }

  if (now - lastSampleAt >= kSampleMs) {
    lastSampleAt = now;
    float rssi;
    if (snifferTakeRssi(rssi)) {
      addReading(rssi, now);
      Serial.printf("track: raw=%.0f filt=%.1f d=%.1fm trend=%d disp=%d\n", rssi,
                    filter.value(), estimateDistance(filter.value(), model()),
                    static_cast<int>(trend.state()), snifferClients(kClientWindowMs));
    }
  }
  if (now - lastSeenAt > kLostAfterMs) {
    snifferStop();  // look for it on every channel
    trend.reset();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(kButtonPin, INPUT_PULLUP);

  uiBegin();
  uiSplash();
  const uint32_t splashAt = millis();

  prefs.begin("radar", false);
  rssiAt1m = prefs.getFloat("a1m", kDefaultRssiAt1m);
  outdoor = prefs.getBool("out", false);
  scannerBegin();

  while (millis() - splashAt < kSplashMs) delay(10);
  Serial.println("radar-wifi listo");
}

void loop() {
  const uint32_t now = millis();

  const Gesture g = button.update(digitalRead(kButtonPin) == LOW, now);
  if (g != Gesture::None) handleAction(nav.onGesture(g, nets.size()), now);

  updateRadio(now);

  if (toast && static_cast<int32_t>(now - toastUntil) >= 0) toast = nullptr;

  if (now - lastFrameAt >= kFrameMs) {
    lastFrameAt = now;
    const Signal signal = signalOk(now) ? Signal::Ok
                          : everSeen    ? Signal::Lost
                                        : Signal::Searching;
    TrackView view{&target,
                   signal,
                   filter.value(),
                   distanceRange(filter.value(), filter.sigma(), model()),
                   trend.state(),
                   snifferActive() ? snifferClients(kClientWindowMs) : -1};
    uiDraw(nav, nets, scannerBusy() && !tracking(), view, outdoor, rssiAt1m,
           button.heldMs(now), button.pending(now), toast, now);
  }
  delay(5);
}
