/* TrackBridge additions to LoRa APRS Tracker.
 * Based on LoRa APRS Tracker, Copyright (C) 2026 Ricardo Guzman - CA2RXU.
 *
 * This file is part of LoRa APRS Tracker (TrackBridge edition).
 *
 * LoRa APRS Tracker is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LoRa APRS Tracker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LoRa APRS Tracker. If not, see <https://www.gnu.org/licenses/>.
 */

// What the tracker may transmit on its own, depending on the TNC mode chosen in the app. Plain C++ (no Arduino), so the
// whole table is tested on a PC. Packets the phone writes over Bluetooth are always transmitted; this is only about
// what the tracker itself originates.
//
//   Normal   the tracker does everything itself (stock behaviour)
//   Full     the tracker originates nothing: it is only the radio for the phone
//   Semi     the tracker keeps sending its own beacons; messages are the phone's job while a phone is connected, and
//            the tracker's own (store + ack) while none is
//   Smart    Full while a phone is connected, Semi otherwise (switching back a little after the phone is gone)

#ifndef TB_TNC_H_
#define TB_TNC_H_

#include <stdint.h>

namespace TB {

enum TncMode { kTncNormal = 0, kTncFull = 1, kTncSemi = 2, kTncSmart = 3 };

// After the phone disconnects, Smart mode waits this long before the tracker beacons again (a short Bluetooth drop
// must not cause a beacon burst).
constexpr uint32_t kTncResumeDelayMs = 15u * 1000u;

struct TxPolicy {
    bool beacons;      // position beacons, status and telemetry packets
    bool messaging;    // answer (ack, pong), store and show messages addressed to the tracker
    bool digipeat;     // repeat other stations' packets when the digipeater is switched on
    bool manualTx;     // messages and beacons sent from the tracker's own menu / keyboard
};

// Any other number is treated as Normal.
TncMode tncModeFrom(int value);

// The mode in effect right now: Smart is Full with a phone and Semi without, the others are themselves.
TncMode effectiveTncMode(TncMode mode, bool phoneConnected, uint32_t msSinceDisconnect);

// [msSinceDisconnect] is only looked at when no phone is connected (a huge value when none was ever connected).

TxPolicy txPolicyFor(TncMode mode, bool phoneConnected, uint32_t msSinceDisconnect);

}  // namespace TB

#endif
