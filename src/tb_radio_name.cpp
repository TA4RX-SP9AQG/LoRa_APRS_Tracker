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
#include "tb_lang.h"

// Screen label of a radio profile (see tb_radio.h). The setting that Europe and Turkey share is called EU in the English
// build and TR in the Turkish one.
String tbRadioName(const LoraType& lora, bool* known, size_t maxChars) {
    char buf[16];
    TB::radioLabel(buf, sizeof(buf), lora.frequency, lora.signalBandwidth, lora.spreadingFactor, lora.codingRate4, TB_T("EU", "TR"), maxChars);
    if (known) *known = TB::radioRegion(lora.frequency, lora.signalBandwidth, lora.spreadingFactor, lora.codingRate4) != TB::kRegionUnknown;
    return String(buf);
}
