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

#include "tb_tnc.h"

namespace TB {

TncMode tncModeFrom(int value) {
    switch (value) {
        case 1: return kTncFull;
        case 2: return kTncSemi;
        case 3: return kTncSmart;
        default: return kTncNormal;
    }
}

TncMode effectiveTncMode(TncMode mode, bool phoneConnected, uint32_t msSinceDisconnect) {
    (void)msSinceDisconnect;
    if (mode != kTncSmart) return mode;
    return phoneConnected ? kTncFull : kTncSemi;
}

TxPolicy txPolicyFor(TncMode mode, bool phoneConnected, uint32_t msSinceDisconnect) {
    TxPolicy p;
    if (mode == kTncSmart && !phoneConnected && msSinceDisconnect < kTncResumeDelayMs) {
        // The phone has just gone: answer messages right away, but wait a little before beaconing again.
        p.beacons = false;
        p.messaging = true;
        p.digipeat = true;
        p.manualTx = true;
        return p;
    }
    switch (effectiveTncMode(mode, phoneConnected, msSinceDisconnect)) {
        case kTncFull:
            p.beacons = false;
            p.messaging = false;
            p.digipeat = false;
            p.manualTx = false;
            break;
        case kTncSemi:
            p.beacons = true;
            p.messaging = !phoneConnected;    // with a phone connected the app answers messages
            p.digipeat = true;
            p.manualTx = true;
            break;
        default:
            p.beacons = true;
            p.messaging = true;
            p.digipeat = true;
            p.manualTx = true;
            break;
    }
    return p;
}

const char* tncTag(TncMode mode) {
    switch (mode) {
        case kTncFull: return "FULL";
        case kTncSemi: return "SEMI";
        case kTncSmart: return "AUTO";
        default: return "";
    }
}

}  // namespace TB
