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

// Arduino side of changing settings from the TrackBridge app (the rules live in tb_control.cpp): the control
// characteristic only copies the written bytes; everything else happens in the main loop.

#ifndef TB_BLE_CONTROL_H_
#define TB_BLE_CONTROL_H_

#include <NimBLEDevice.h>

namespace TB_BLE_CONTROL {

    // Adds the control (write) and result (read) characteristics to [service].
    void setup(NimBLEService* service);

    // Main loop: handles a received command, ends expired PINs/sessions, restarts after a saved change.
    void loop();

    // Bluetooth task: a phone connected or disconnected (only sets a flag; the loop resets the session).
    void connectionChanged();

    // True while a pairing PIN is on the screen (other full-screen messages must wait).
    bool pinActive();

    // Called by the screen code before it draws: shows the pairing PIN while one is active and returns true.
    bool drawPinScreen();

}

#endif
