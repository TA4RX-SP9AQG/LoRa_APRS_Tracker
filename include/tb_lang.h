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

// Compile-time language choice for text shown on the tracker's own screen.
//
//   TB_T("English text", "Turkce metin")
//
// expands to exactly one string literal, so the English build is byte-for-byte the same program as before and
// the Turkish build (-D TB_LANG_TR, PlatformIO env heltec_wireless_tracker_tr) differs only in those literals.
// The screen has no Turkish letters (g, i, s with marks), so on-screen Turkish is written without them.
// Never wrap text that is transmitted over the radio, used as a file name / key, or compared in code.

#ifndef TB_LANG_H_
#define TB_LANG_H_

#ifdef TB_LANG_TR
    #define TB_T(en, tr) tr
#else
    #define TB_T(en, tr) en
#endif

#endif
