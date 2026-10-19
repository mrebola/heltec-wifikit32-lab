#pragma once

#include <cstdint>

#include "distance.h"
#include "nav.h"
#include "net_list.h"
#include "trend.h"

enum class Signal { Searching, Ok, Lost };

// What the tracking screen shows. Built by main.cpp each frame.
struct TrackView {
  const radar::Network* target;
  Signal signal;
  float rssi;  // filtered
  radar::DistanceRange range;
  radar::Trend trend;
  int clients;  // stations heard recently, -1 while not listening
};

// What the deauth screen shows. Built by main.cpp each frame.
struct DeauthView {
  const radar::Network* target;
  bool running;       // frames are being transmitted
  uint32_t sent;      // frames sent since the run started
  uint32_t elapsedMs; // frozen when stopped, so it shows how long it lasted
  int clients;        // stations targeted, -1 while not listening
  int err;            // ESP-IDF error of the last burst, 0 = ok
};

void uiBegin();
// Boot screen with the Hackwise logo.
void uiSplash();
// Draws one frame. `holdMs` drives the press-progress bar (0 = not held) and
// `pending` the "what happens if you let go" panel.
// `toast`, when not null, is drawn as a centered message over the screen.
void uiDraw(const radar::Nav& nav, const radar::NetList& nets, bool scanning,
            const TrackView& track, const DeauthView& deauth, bool outdoor,
            float rssiAt1m, uint32_t holdMs, radar::Gesture pending,
            const char* toast, uint32_t now);
