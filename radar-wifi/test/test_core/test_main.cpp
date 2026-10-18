#include <unity.h>

#include <cmath>
#include <cstring>

#include "button.h"
#include "distance.h"
#include "frames.h"
#include "nav.h"
#include "net_list.h"
#include "rssi_filter.h"
#include "trend.h"

using namespace radar;

void setUp() {}
void tearDown() {}

// ---------- RssiFilter ----------

void test_filter_not_ready_before_first_sample() {
  RssiFilter f;
  TEST_ASSERT_FALSE(f.ready());
}

void test_filter_first_sample_is_passthrough() {
  RssiFilter f;
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -60.0f, f.update(-60));
  TEST_ASSERT_TRUE(f.ready());
}

void test_filter_converges_to_constant_input() {
  RssiFilter f;
  f.update(-80);
  for (int i = 0; i < 50; i++) f.update(-50);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, -50.0f, f.value());
}

void test_filter_rejects_single_spike() {
  RssiFilter f;
  for (int i = 0; i < 10; i++) f.update(-60);
  f.update(-30);  // one bogus reading
  TEST_ASSERT_FLOAT_WITHIN(0.5f, -60.0f, f.value());
}

void test_filter_sigma_has_floor() {
  RssiFilter f;
  for (int i = 0; i < 100; i++) f.update(-60);
  TEST_ASSERT_TRUE(f.sigma() >= 3.0f);
}

void test_filter_reset() {
  RssiFilter f;
  for (int i = 0; i < 5; i++) f.update(-60);
  f.reset();
  TEST_ASSERT_FALSE(f.ready());
  TEST_ASSERT_EQUAL(0, f.samples());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -70.0f, f.update(-70));  // old window ignored
}

void test_filter_counts_samples() {
  RssiFilter f;
  for (int i = 0; i < 7; i++) f.update(-60);
  TEST_ASSERT_EQUAL(7, f.samples());
}

void test_filter_even_window_spike() {
  RssiFilter f;
  f.update(-60);
  f.update(-20);  // 2 samples: median is the mean of both
  TEST_ASSERT_TRUE(f.value() > -60.0f);
  f.update(-60);
  f.update(-60);  // 4 samples: spike falls out of the middle pair
  TEST_ASSERT_TRUE(f.value() < -45.0f);
}

// ---------- Distance ----------

void test_distance_at_reference_is_one_meter() {
  DistanceModel m{-40.0f, 3.0f};
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, estimateDistance(-40, m));
}

void test_distance_ten_n_db_is_ten_meters() {
  DistanceModel m{-40.0f, 3.0f};
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, estimateDistance(-70, m));
}

void test_distance_is_clamped() {
  DistanceModel m{-40.0f, 2.0f};
  TEST_ASSERT_TRUE(estimateDistance(0, m) >= kMinDistance);
  TEST_ASSERT_TRUE(estimateDistance(-200, m) <= kMaxDistance);
}

void test_distance_range_brackets_estimate() {
  DistanceModel m{-40.0f, 3.0f};
  DistanceRange r = distanceRange(-70, 3.0f, m);
  TEST_ASSERT_TRUE(r.min < r.est);
  TEST_ASSERT_TRUE(r.est < r.max);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, estimateDistance(-67, m), r.min);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, estimateDistance(-73, m), r.max);
}

// ---------- Trend ----------

static Trend feedRamp(TrendTracker& t, float startDb, float dbPerSec, int n) {
  Trend last = Trend::Stable;
  for (int i = 0; i < n; i++) {
    uint32_t ms = i * 200;
    last = t.add(ms, startDb + dbPerSec * ms / 1000.0f);
  }
  return last;
}

void test_trend_stable_on_flat_signal() {
  TrendTracker t;
  TEST_ASSERT_EQUAL(Trend::Stable, feedRamp(t, -60, 0.0f, 20));
}

void test_trend_approaching_on_rising_rssi() {
  TrendTracker t;
  TEST_ASSERT_EQUAL(Trend::Approaching, feedRamp(t, -70, 2.0f, 20));
}

void test_trend_receding_on_falling_rssi() {
  TrendTracker t;
  TEST_ASSERT_EQUAL(Trend::Receding, feedRamp(t, -50, -2.0f, 20));
}

void test_trend_needs_history() {
  TrendTracker t;
  // Only 3 samples spanning 0.4 s: not enough to call a trend.
  TEST_ASSERT_EQUAL(Trend::Stable, feedRamp(t, -70, 10.0f, 3));
}

void test_trend_hysteresis_holds_state() {
  TrendTracker t;
  feedRamp(t, -70, 2.0f, 20);  // now Approaching
  // Slope of 0.35 dB/s: below the enter threshold, above the exit one.
  Trend last = Trend::Stable;
  for (int i = 20; i < 60; i++) {
    uint32_t ms = i * 200;
    last = t.add(ms, -62.0f + 0.35f * (ms - 4000) / 1000.0f);
  }
  TEST_ASSERT_EQUAL(Trend::Approaching, last);
}

void test_trend_flips_directly_to_receding() {
  TrendTracker t;
  feedRamp(t, -70, 2.0f, 20);  // Approaching, ends at t=3.8 s, -62.4 dBm
  Trend last = Trend::Approaching;
  for (int i = 20; i < 40; i++) {
    uint32_t ms = i * 200;
    last = t.add(ms, -62.4f - 3.0f * (ms - 3800) / 1000.0f);
  }
  TEST_ASSERT_EQUAL(Trend::Receding, last);
}

void test_trend_ignores_samples_older_than_window() {
  TrendTracker t;
  feedRamp(t, -70, 2.0f, 20);
  // 10 s gap, then two fresh samples: too little history for a trend.
  t.add(15000, -60);
  TEST_ASSERT_EQUAL(Trend::Stable, t.add(15200, -55));
}

void test_trend_survives_millis_wrap() {
  TrendTracker t;
  const uint32_t start = 0xFFFFFFFFu - 1500;  // wraps half way through
  Trend last = Trend::Stable;
  for (int i = 0; i < 20; i++) {
    const uint32_t ms = start + i * 200;
    last = t.add(ms, -70.0f + 2.0f * i * 0.2f);
  }
  TEST_ASSERT_EQUAL(Trend::Approaching, last);
}

void test_trend_reset() {
  TrendTracker t;
  feedRamp(t, -70, 2.0f, 20);
  t.reset();
  TEST_ASSERT_EQUAL(Trend::Stable, t.add(10000, -60));
}

// ---------- NetList ----------

static Network makeNet(uint8_t id, int8_t rssi, const char* ssid = "x") {
  Network n{};
  for (int i = 0; i < 6; i++) n.bssid[i] = id;
  std::strncpy(n.ssid, ssid, sizeof(n.ssid) - 1);
  n.rssi = rssi;
  n.channel = 6;
  return n;
}

void test_netlist_sorts_strongest_first() {
  NetList l;
  l.add(makeNet(1, -80));
  l.add(makeNet(2, -40));
  l.add(makeNet(3, -60));
  l.sort();
  TEST_ASSERT_EQUAL(3, l.size());
  TEST_ASSERT_EQUAL(-40, l.at(0).rssi);
  TEST_ASSERT_EQUAL(-60, l.at(1).rssi);
  TEST_ASSERT_EQUAL(-80, l.at(2).rssi);
}

void test_netlist_merges_same_bssid_keeping_strongest() {
  NetList l;
  l.add(makeNet(1, -80));
  l.add(makeNet(1, -50));
  l.add(makeNet(1, -70));
  TEST_ASSERT_EQUAL(1, l.size());
  TEST_ASSERT_EQUAL(-50, l.at(0).rssi);
}

void test_netlist_full_keeps_strongest() {
  NetList l;
  for (int i = 0; i < NetList::kCapacity; i++) l.add(makeNet(i + 1, -90));
  l.add(makeNet(200, -30));  // stronger than everyone: must get in
  l.add(makeNet(201, -95));  // weaker than everyone: dropped
  l.sort();
  TEST_ASSERT_EQUAL(NetList::kCapacity, l.size());
  TEST_ASSERT_EQUAL(-30, l.at(0).rssi);
  TEST_ASSERT_EQUAL(-1, l.indexOf(makeNet(201, -95).bssid));
}

void test_netlist_full_merges_existing_bssid() {
  NetList l;
  for (int i = 0; i < NetList::kCapacity; i++) l.add(makeNet(i + 1, -90));
  l.add(makeNet(5, -40));  // already present: merge, don't evict anyone
  l.sort();
  TEST_ASSERT_EQUAL(NetList::kCapacity, l.size());
  TEST_ASSERT_EQUAL(0, l.indexOf(makeNet(5, 0).bssid));
  for (int i = 0; i < NetList::kCapacity; i++) {
    TEST_ASSERT_TRUE(l.indexOf(makeNet(i + 1, 0).bssid) >= 0);
  }
}

void test_netlist_index_of() {
  NetList l;
  l.add(makeNet(1, -80));
  l.add(makeNet(2, -40));
  l.sort();
  TEST_ASSERT_EQUAL(1, l.indexOf(makeNet(1, 0).bssid));
  TEST_ASSERT_EQUAL(-1, l.indexOf(makeNet(9, 0).bssid));
}

void test_netlist_ssid_always_terminated() {
  Network n = makeNet(1, -50, "0123456789012345678901234567890123456789");
  TEST_ASSERT_EQUAL('\0', n.ssid[sizeof(n.ssid) - 1]);
}

// ---------- Button ----------

// Holds the button for `holdMs` and releases it; returns every gesture seen.
static Gesture press(Button& b, uint32_t& now, uint32_t holdMs) {
  Gesture seen = Gesture::None;
  for (uint32_t t = 0; t <= holdMs; t += 5) {
    Gesture g = b.update(true, now + t);
    if (g != Gesture::None) seen = g;
  }
  now += holdMs;
  for (uint32_t t = 5; t <= 100; t += 5) {
    Gesture g = b.update(false, now + t);
    if (g != Gesture::None) seen = g;
  }
  now += 100;
  return seen;
}

void test_button_short_press() {
  Button b;
  uint32_t now = 1000;
  TEST_ASSERT_EQUAL(Gesture::Short, press(b, now, 200));
}

void test_button_long_press() {
  Button b;
  uint32_t now = 1000;
  TEST_ASSERT_EQUAL(Gesture::Long, press(b, now, 1000));
}

void test_button_very_long_press() {
  Button b;
  uint32_t now = 1000;
  TEST_ASSERT_EQUAL(Gesture::VeryLong, press(b, now, 2500));
}

void test_button_very_long_fires_while_held_and_only_once() {
  Button b;
  int count = 0;
  for (uint32_t t = 0; t <= 4000; t += 5) {
    if (b.update(true, t) == Gesture::VeryLong) count++;
  }
  TEST_ASSERT_EQUAL(1, count);
  for (uint32_t t = 4005; t <= 4200; t += 5) {
    TEST_ASSERT_EQUAL(Gesture::None, b.update(false, t));
  }
}

void test_button_boundaries() {
  Button b;
  uint32_t now = 1000;
  TEST_ASSERT_EQUAL(Gesture::Short, press(b, now, Button::kLongMs - 50));
  TEST_ASSERT_EQUAL(Gesture::Long, press(b, now, Button::kLongMs + 50));
  TEST_ASSERT_EQUAL(Gesture::Long, press(b, now, Button::kVeryLongMs - 50));
  TEST_ASSERT_EQUAL(Gesture::VeryLong, press(b, now, Button::kVeryLongMs + 50));
}

void test_button_ignores_bounce() {
  Button b;
  // 10 ms glitch, shorter than the debounce time.
  TEST_ASSERT_EQUAL(Gesture::None, b.update(true, 0));
  TEST_ASSERT_EQUAL(Gesture::None, b.update(true, 10));
  for (uint32_t t = 15; t < 500; t += 5) {
    TEST_ASSERT_EQUAL(Gesture::None, b.update(false, t));
  }
}

void test_button_survives_millis_wrap() {
  Button b;
  uint32_t now = 0xFFFFFFFFu - 100;  // every press crosses the wrap
  TEST_ASSERT_EQUAL(Gesture::Short, press(b, now, 200));
  now = 0xFFFFFFFFu - 300;
  TEST_ASSERT_EQUAL(Gesture::Long, press(b, now, 1000));
  now = 0xFFFFFFFFu - 1000;
  TEST_ASSERT_EQUAL(Gesture::VeryLong, press(b, now, 2500));
}

void test_button_hold_progress() {
  Button b;
  for (uint32_t t = 0; t <= 1000; t += 5) b.update(true, t);
  TEST_ASSERT_TRUE(b.held());
  TEST_ASSERT_UINT32_WITHIN(30, 1000, b.heldMs(1000));
}

// ---------- Nav ----------

void test_nav_list_short_moves_cursor_and_wraps() {
  Nav n;
  TEST_ASSERT_EQUAL(Action::None, n.onGesture(Gesture::Short, 3));
  TEST_ASSERT_EQUAL(1, n.cursor);
  n.onGesture(Gesture::Short, 3);
  n.onGesture(Gesture::Short, 3);
  TEST_ASSERT_EQUAL(0, n.cursor);
}

void test_nav_list_long_starts_tracking() {
  Nav n;
  TEST_ASSERT_EQUAL(Action::StartTrack, n.onGesture(Gesture::Long, 3));
  TEST_ASSERT_EQUAL(Screen::Track, n.screen);
}

void test_nav_list_long_on_empty_does_nothing() {
  Nav n;
  TEST_ASSERT_EQUAL(Action::None, n.onGesture(Gesture::Long, 0));
  TEST_ASSERT_EQUAL(Screen::List, n.screen);
}

void test_nav_list_very_long_rescans() {
  Nav n;
  TEST_ASSERT_EQUAL(Action::Rescan, n.onGesture(Gesture::VeryLong, 3));
}

void test_nav_track_very_long_returns_to_list() {
  Nav n;
  n.onGesture(Gesture::Long, 3);
  TEST_ASSERT_EQUAL(Action::StopTrack, n.onGesture(Gesture::VeryLong, 3));
  TEST_ASSERT_EQUAL(Screen::List, n.screen);
}

void test_nav_menu_flow() {
  Nav n;
  n.onGesture(Gesture::Long, 3);  // Track
  TEST_ASSERT_EQUAL(Action::None, n.onGesture(Gesture::Long, 3));
  TEST_ASSERT_EQUAL(Screen::Menu, n.screen);
  TEST_ASSERT_EQUAL(MenuItem::Calibrate, n.menuItem());

  n.onGesture(Gesture::Short, 3);
  TEST_ASSERT_EQUAL(MenuItem::Environment, n.menuItem());
  TEST_ASSERT_EQUAL(Action::ToggleEnv, n.onGesture(Gesture::Long, 3));
  TEST_ASSERT_EQUAL(Screen::Menu, n.screen);  // stays to show the new value

  n.onGesture(Gesture::Short, 3);
  TEST_ASSERT_EQUAL(MenuItem::Back, n.menuItem());
  TEST_ASSERT_EQUAL(Action::None, n.onGesture(Gesture::Long, 3));
  TEST_ASSERT_EQUAL(Screen::Track, n.screen);
}

void test_nav_menu_calibrate_returns_to_track() {
  Nav n;
  n.onGesture(Gesture::Long, 3);
  n.onGesture(Gesture::Long, 3);
  TEST_ASSERT_EQUAL(Action::Calibrate, n.onGesture(Gesture::Long, 3));
  TEST_ASSERT_EQUAL(Screen::Track, n.screen);
}

void test_nav_menu_very_long_goes_back_to_track() {
  Nav n;
  n.onGesture(Gesture::Long, 3);
  n.onGesture(Gesture::Long, 3);
  n.onGesture(Gesture::Short, 3);
  n.onGesture(Gesture::VeryLong, 3);
  TEST_ASSERT_EQUAL(Screen::Track, n.screen);
  n.onGesture(Gesture::Long, 3);  // reopening the menu starts at the top
  TEST_ASSERT_EQUAL(MenuItem::Calibrate, n.menuItem());
}

void test_nav_scroll_keeps_cursor_visible() {
  Nav n;
  for (int i = 0; i < 7; i++) n.onGesture(Gesture::Short, 10);
  TEST_ASSERT_EQUAL(7, n.cursor);
  TEST_ASSERT_TRUE(n.cursor >= n.top && n.cursor < n.top + Nav::kRows);
  n.setCursor(0, 10);
  TEST_ASSERT_EQUAL(0, n.top);
}

void test_nav_clamps_cursor_when_list_shrinks() {
  Nav n;
  for (int i = 0; i < 7; i++) n.onGesture(Gesture::Short, 10);
  n.setCursor(n.cursor, 2);
  TEST_ASSERT_EQUAL(1, n.cursor);
  TEST_ASSERT_EQUAL(0, n.top);
  n.setCursor(n.cursor, 0);
  TEST_ASSERT_EQUAL(0, n.cursor);
  TEST_ASSERT_EQUAL(0, n.top);
}

void test_nav_top_fills_screen_when_list_shrinks() {
  Nav n;
  n.setCursor(19, 20);
  TEST_ASSERT_EQUAL(20 - Nav::kRows, n.top);
  n.setCursor(16, 17);  // same AP moved up after a rescan
  TEST_ASSERT_EQUAL(16, n.cursor);
  TEST_ASSERT_EQUAL(17 - Nav::kRows, n.top);  // no empty rows at the bottom
}

// ---------- Frames ----------

static const uint8_t kAp[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
static const uint8_t kSta[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
static const uint8_t kBcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// 24-byte 802.11 header with the given frame control and addresses.
static void header(uint8_t* f, uint8_t fc0, uint8_t fc1, const uint8_t* a1,
                   const uint8_t* a2, const uint8_t* a3) {
  std::memset(f, 0, 24);
  f[0] = fc0;
  f[1] = fc1;
  std::memcpy(f + 4, a1, 6);
  std::memcpy(f + 10, a2, 6);
  std::memcpy(f + 16, a3, 6);
}

void test_sniffer_beacon_is_from_ap() {
  uint8_t f[24];
  header(f, 0x80, 0x00, kBcast, kAp, kAp);  // mgmt / beacon
  FrameInfo info;
  TEST_ASSERT_TRUE(parseFrame(f, sizeof(f), kAp, info));
  TEST_ASSERT_TRUE(info.fromAp);
  TEST_ASSERT_FALSE(info.hasClient);
}

void test_sniffer_data_to_ap_gives_client() {
  uint8_t f[24];
  header(f, 0x08, 0x01, kAp, kSta, kBcast);  // data, ToDS
  FrameInfo info;
  TEST_ASSERT_TRUE(parseFrame(f, sizeof(f), kAp, info));
  TEST_ASSERT_FALSE(info.fromAp);
  TEST_ASSERT_TRUE(info.hasClient);
  TEST_ASSERT_EQUAL_MEMORY(kSta, info.client, 6);
}

void test_sniffer_data_from_ap_gives_client() {
  uint8_t f[24];
  header(f, 0x88, 0x02, kSta, kAp, kAp);  // QoS data, FromDS
  FrameInfo info;
  TEST_ASSERT_TRUE(parseFrame(f, sizeof(f), kAp, info));
  TEST_ASSERT_TRUE(info.fromAp);
  TEST_ASSERT_TRUE(info.hasClient);
  TEST_ASSERT_EQUAL_MEMORY(kSta, info.client, 6);
}

void test_sniffer_multicast_from_ap_has_no_client() {
  uint8_t f[24];
  header(f, 0x08, 0x02, kBcast, kAp, kAp);
  FrameInfo info;
  TEST_ASSERT_TRUE(parseFrame(f, sizeof(f), kAp, info));
  TEST_ASSERT_TRUE(info.fromAp);
  TEST_ASSERT_FALSE(info.hasClient);
}

void test_sniffer_mgmt_from_client_gives_client() {
  uint8_t f[24];
  header(f, 0x00, 0x00, kAp, kSta, kAp);  // association request
  FrameInfo info;
  TEST_ASSERT_TRUE(parseFrame(f, sizeof(f), kAp, info));
  TEST_ASSERT_FALSE(info.fromAp);
  TEST_ASSERT_TRUE(info.hasClient);
}

void test_sniffer_ignores_other_networks_and_junk() {
  uint8_t other[6] = {1, 2, 3, 4, 5, 6};
  uint8_t f[24];
  FrameInfo info;
  header(f, 0x80, 0x00, kBcast, other, other);
  TEST_ASSERT_FALSE(parseFrame(f, sizeof(f), kAp, info));
  header(f, 0x08, 0x03, kAp, kSta, kAp);  // WDS: ToDS + FromDS
  TEST_ASSERT_FALSE(parseFrame(f, sizeof(f), kAp, info));
  header(f, 0xB4, 0x00, kAp, kSta, kAp);  // control frame (RTS)
  TEST_ASSERT_FALSE(parseFrame(f, sizeof(f), kAp, info));
  TEST_ASSERT_FALSE(parseFrame(f, 10, kAp, info));  // truncated
}

void test_clients_counts_unique_macs() {
  ClientTable t;
  uint8_t a[6] = {2, 0, 0, 0, 0, 1}, b[6] = {2, 0, 0, 0, 0, 2};
  t.seen(a, 1000);
  t.seen(a, 1100);
  t.seen(b, 1200);
  TEST_ASSERT_EQUAL(2, t.count(1300, 60000));
}

void test_clients_expire_after_window() {
  ClientTable t;
  uint8_t a[6] = {2, 0, 0, 0, 0, 1}, b[6] = {2, 0, 0, 0, 0, 2};
  t.seen(a, 1000);
  t.seen(b, 50000);
  TEST_ASSERT_EQUAL(1, t.count(62000, 60000));
}

void test_clients_full_replaces_oldest() {
  ClientTable t;
  uint8_t m[6] = {2, 0, 0, 0, 0, 0};
  for (int i = 0; i < ClientTable::kCapacity; i++) {
    m[5] = static_cast<uint8_t>(i);
    t.seen(m, 1000 + i);
  }
  m[5] = 200;
  t.seen(m, 5000);
  TEST_ASSERT_EQUAL(ClientTable::kCapacity, t.count(5000, 60000));
  m[5] = 0;  // the oldest one was evicted
  TEST_ASSERT_FALSE(t.contains(m));
}

void test_clients_survive_millis_wrap() {
  ClientTable t;
  uint8_t a[6] = {2, 0, 0, 0, 0, 1};
  t.seen(a, 0xFFFFFFFFu - 1000);
  TEST_ASSERT_EQUAL(1, t.count(2000, 60000));
}

void test_clients_clear() {
  ClientTable t;
  uint8_t a[6] = {2, 0, 0, 0, 0, 1};
  t.seen(a, 1000);
  t.clear();
  TEST_ASSERT_EQUAL(0, t.count(1000, 60000));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_filter_not_ready_before_first_sample);
  RUN_TEST(test_filter_first_sample_is_passthrough);
  RUN_TEST(test_filter_converges_to_constant_input);
  RUN_TEST(test_filter_rejects_single_spike);
  RUN_TEST(test_filter_sigma_has_floor);
  RUN_TEST(test_filter_reset);
  RUN_TEST(test_filter_counts_samples);
  RUN_TEST(test_filter_even_window_spike);
  RUN_TEST(test_distance_at_reference_is_one_meter);
  RUN_TEST(test_distance_ten_n_db_is_ten_meters);
  RUN_TEST(test_distance_is_clamped);
  RUN_TEST(test_distance_range_brackets_estimate);
  RUN_TEST(test_trend_stable_on_flat_signal);
  RUN_TEST(test_trend_approaching_on_rising_rssi);
  RUN_TEST(test_trend_receding_on_falling_rssi);
  RUN_TEST(test_trend_needs_history);
  RUN_TEST(test_trend_hysteresis_holds_state);
  RUN_TEST(test_trend_flips_directly_to_receding);
  RUN_TEST(test_trend_ignores_samples_older_than_window);
  RUN_TEST(test_trend_survives_millis_wrap);
  RUN_TEST(test_trend_reset);
  RUN_TEST(test_netlist_sorts_strongest_first);
  RUN_TEST(test_netlist_merges_same_bssid_keeping_strongest);
  RUN_TEST(test_netlist_full_keeps_strongest);
  RUN_TEST(test_netlist_full_merges_existing_bssid);
  RUN_TEST(test_netlist_index_of);
  RUN_TEST(test_netlist_ssid_always_terminated);
  RUN_TEST(test_button_short_press);
  RUN_TEST(test_button_long_press);
  RUN_TEST(test_button_very_long_press);
  RUN_TEST(test_button_very_long_fires_while_held_and_only_once);
  RUN_TEST(test_button_boundaries);
  RUN_TEST(test_button_ignores_bounce);
  RUN_TEST(test_button_survives_millis_wrap);
  RUN_TEST(test_button_hold_progress);
  RUN_TEST(test_nav_list_short_moves_cursor_and_wraps);
  RUN_TEST(test_nav_list_long_starts_tracking);
  RUN_TEST(test_nav_list_long_on_empty_does_nothing);
  RUN_TEST(test_nav_list_very_long_rescans);
  RUN_TEST(test_nav_track_very_long_returns_to_list);
  RUN_TEST(test_nav_menu_flow);
  RUN_TEST(test_nav_menu_calibrate_returns_to_track);
  RUN_TEST(test_nav_menu_very_long_goes_back_to_track);
  RUN_TEST(test_nav_scroll_keeps_cursor_visible);
  RUN_TEST(test_nav_clamps_cursor_when_list_shrinks);
  RUN_TEST(test_nav_top_fills_screen_when_list_shrinks);
  RUN_TEST(test_sniffer_beacon_is_from_ap);
  RUN_TEST(test_sniffer_data_to_ap_gives_client);
  RUN_TEST(test_sniffer_data_from_ap_gives_client);
  RUN_TEST(test_sniffer_multicast_from_ap_has_no_client);
  RUN_TEST(test_sniffer_mgmt_from_client_gives_client);
  RUN_TEST(test_sniffer_ignores_other_networks_and_junk);
  RUN_TEST(test_clients_counts_unique_macs);
  RUN_TEST(test_clients_expire_after_window);
  RUN_TEST(test_clients_full_replaces_oldest);
  RUN_TEST(test_clients_survive_millis_wrap);
  RUN_TEST(test_clients_clear);
  return UNITY_END();
}
