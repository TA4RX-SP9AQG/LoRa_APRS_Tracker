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

// The name the tracker shows for a radio setting. The stock firmware wrote a fixed name per profile *position*
// (first = EU, second = PL ...), so a setting changed from the app still showed the old name. Here the name follows the
// actual values: frequency, bandwidth, spreading factor and coding rate.
//
// Plain C++ (no Arduino), tested on a PC. The Arduino wrapper tbRadioName() is declared at the end.

#ifndef TB_RADIO_H_
#define TB_RADIO_H_

#include <stddef.h>

namespace TB {

enum RadioRegion {
    kRegionUnknown = 0,
    kRegionEuTr,    // 433.775 MHz, SF12, CR 4/5, 125 kHz: Europe, and also the setting used in Turkey
    kRegionPl,      // 434.855 MHz, SF9, CR 4/7, 125 kHz
    kRegionUk,      // 439.9125 MHz, SF12, CR 4/5, 125 kHz
    kRegionUs       // 915 MHz, SF12, CR 4/5, 125 kHz
};

RadioRegion radioRegion(long freqHz, long bwHz, int sf, int cr);

// Writes the country code of a known setting, or the frequency in MHz ("433.9") when it is not one. [sharedName] is the
// code for the setting that Europe and Turkey share ("EU" in the English build, "TR" in the Turkish one). When the
// frequency text would be longer than [maxChars], decimals are cut off (not rounded) until it fits. Returns the length, 0 if [cap] is
// too small (at least 12 is needed).
size_t radioLabel(char* out, size_t cap, long freqHz, long bwHz, int sf, int cr, const char* sharedName, size_t maxChars = 11);

}  // namespace TB

#ifdef ARDUINO
#include <Arduino.h>
#include "configuration.h"

// Label of a radio profile for the tracker's screen. [known] (optional) tells whether it is a recognised setting.
String tbRadioName(const LoraType& lora, bool* known = nullptr, size_t maxChars = 11);
#endif

#endif
