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

void uiBegin();
// Boot screen with the Hackwise logo.
void uiSplash();
// Draws one frame. `holdMs` drives the press-progress bar (0 = not held) and
// `pending` the "what happens if you let go" panel.
// `toast`, when not null, is drawn as a centered message over the screen.
void uiDraw(const radar::Nav& nav, const radar::NetList& nets, bool scanning,
            const TrackView& track, bool outdoor, float rssiAt1m,
            uint32_t holdMs, radar::Gesture pending, const char* toast,
            uint32_t now);
