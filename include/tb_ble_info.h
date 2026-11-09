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

// Read-only "device info" Bluetooth service for the TrackBridge app: status (battery, GPS, uptime, ...),
// a summary of the settings, and the messages stored on the tracker. Nothing here can change the tracker.
// The JSON is built from the main loop (never inside a Bluetooth callback), so it cannot race with the
// rest of the firmware.

#ifndef TB_BLE_INFO_H_
#define TB_BLE_INFO_H_

#include <NimBLEDevice.h>

namespace TB_BLE {

    // Creates the service on [server]. Call once, before advertising starts.
    void setup(NimBLEServer* server);

    // Call from the main loop; refreshes the values at most every few seconds and only while a phone is connected.
    void loop();

}

#endif
