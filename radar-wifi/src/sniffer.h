#pragma once

#include <cstdint>

#include "net_list.h"

// Passive listening on the target's channel (promiscuous mode). Gives many
// more RSSI readings than scanning (every beacon, ~10/s) and counts the
// stations exchanging frames with the AP. Only headers are read.
// Must not run at the same time as a scan.

// Starts listening. `keepClients` keeps the station count of the same AP,
// e.g. after it was found again on a new channel.
void snifferStart(const radar::Network& target, bool keepClients);
void snifferStop();
bool snifferActive();
// Average RSSI of the AP's frames received since the last call. Returns
// false when none arrived.
bool snifferTakeRssi(float& rssi);
// Stations heard in the last `windowMs`.
int snifferClients(uint32_t windowMs);
