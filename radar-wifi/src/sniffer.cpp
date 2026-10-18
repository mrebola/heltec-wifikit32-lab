#include "sniffer.h"

#include <Arduino.h>
#include <esp_wifi.h>

#include <cstring>

#include "frames.h"

namespace {

// Shared between the WiFi task (callback) and loop(); guarded by `lock`.
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
uint8_t bssid[6];
int32_t rssiSum = 0;
int rssiCount = 0;
radar::ClientTable clients;

bool active = false;

// Runs in the WiFi task for every received frame: keep it short.
void onFrame(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
  const auto* pkt = static_cast<const wifi_promiscuous_pkt_t*>(buf);
  radar::FrameInfo info;
  if (!radar::parseFrame(pkt->payload, pkt->rx_ctrl.sig_len, bssid, info)) return;

  const uint32_t now = millis();
  portENTER_CRITICAL(&lock);
  if (info.fromAp) {
    rssiSum += pkt->rx_ctrl.rssi;
    rssiCount++;
  }
  if (info.hasClient) clients.seen(info.client, now);
  portEXIT_CRITICAL(&lock);
}

}  // namespace

void snifferStart(const radar::Network& target, bool keepClients) {
  if (active) snifferStop();
  portENTER_CRITICAL(&lock);
  std::memcpy(bssid, target.bssid, sizeof(bssid));
  rssiSum = 0;
  rssiCount = 0;
  if (!keepClients) clients.clear();
  portEXIT_CRITICAL(&lock);

  wifi_promiscuous_filter_t filter = {
      .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(onFrame);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(target.channel, WIFI_SECOND_CHAN_NONE);
  active = true;
}

void snifferStop() {
  if (!active) return;
  esp_wifi_set_promiscuous(false);
  active = false;
}

bool snifferActive() { return active; }

bool snifferTakeRssi(float& rssi) {
  portENTER_CRITICAL(&lock);
  const int n = rssiCount;
  const int32_t sum = rssiSum;
  rssiSum = 0;
  rssiCount = 0;
  portEXIT_CRITICAL(&lock);
  if (n == 0) return false;
  rssi = static_cast<float>(sum) / n;
  return true;
}

int snifferClients(uint32_t windowMs) {
  const uint32_t now = millis();
  portENTER_CRITICAL(&lock);
  const int n = clients.count(now, windowMs);
  portEXIT_CRITICAL(&lock);
  return n;
}

int snifferCopyClients(uint8_t out[][6], int max, uint32_t windowMs) {
  const uint32_t now = millis();
  portENTER_CRITICAL(&lock);
  const int n = clients.copyAlive(now, windowMs, out, max);
  portEXIT_CRITICAL(&lock);
  return n;
}
