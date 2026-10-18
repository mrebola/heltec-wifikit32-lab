#pragma once

#include "net_list.h"

// Non-blocking wrapper around the ESP32 WiFi scanner (all channels).
enum class ScanResult { Running, Failed, Done };

void scannerBegin();
bool scannerBusy();
void scannerStart();
// Call often. When a scan has finished, copies its results into `out`
// (cleared first), sorted strongest first.
ScanResult scannerPoll(radar::NetList& out);
