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

#include "tb_radio.h"

#include <stdio.h>
#include <string.h>

namespace TB {

RadioRegion radioRegion(long freqHz, long bwHz, int sf, int cr) {
    if (bwHz != 125000) return kRegionUnknown;
    if (freqHz == 433775000L && sf == 12 && cr == 5) return kRegionEuTr;
    if (freqHz == 434855000L && sf == 9 && cr == 7) return kRegionPl;
    if (freqHz == 439912500L && sf == 12 && cr == 5) return kRegionUk;
    if (freqHz == 915000000L && sf == 12 && cr == 5) return kRegionUs;
    return kRegionUnknown;
}

namespace {

// 433900000 -> "433.9", 439912500 -> "439.9125", 915000000 -> "915.0"; at most [decimals] decimals.
size_t mhzText(char* out, size_t cap, long hz, int decimals) {
    if (hz < 0) hz = 0;
    const long whole = hz / 1000000L;
    long frac = hz % 1000000L;
    char digits[8];
    snprintf(digits, sizeof(digits), "%06ld", frac);
    int n = 6;
    while (n > 1 && digits[n - 1] == '0') n--;    // keep at least one decimal
    if (n > decimals) n = decimals;
    if (decimals <= 0 || n <= 0) return static_cast<size_t>(snprintf(out, cap, "%ld", whole));
    digits[n] = '\0';
    return static_cast<size_t>(snprintf(out, cap, "%ld.%s", whole, digits));
}

}  // namespace

size_t radioLabel(char* out, size_t cap, long freqHz, long bwHz, int sf, int cr, const char* sharedName, size_t maxChars) {
    if (!out || cap < 12) return 0;
    switch (radioRegion(freqHz, bwHz, sf, cr)) {
        case kRegionEuTr: snprintf(out, cap, "%s", sharedName ? sharedName : "EU"); return strlen(out);
        case kRegionPl: snprintf(out, cap, "PL"); return 2;
        case kRegionUk: snprintf(out, cap, "UK"); return 2;
        case kRegionUs: snprintf(out, cap, "US"); return 2;
        default: break;
    }
    for (int decimals = 6; decimals >= 0; decimals--) {
        const size_t n = mhzText(out, cap, freqHz, decimals);
        if (n <= maxChars || decimals == 0) return n;
    }
    return 0;
}

}  // namespace TB
