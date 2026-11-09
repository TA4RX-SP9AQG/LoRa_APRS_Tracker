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

// Plain-data description of what the tracker tells the TrackBridge app over Bluetooth, and the functions
// that turn it into compact JSON. This file and tb_info.cpp contain no Arduino code on purpose, so they
// are unit-tested on a PC (tools/tb_test). Only whitelisted fields exist here: there is simply no field
// for passwords, e-mail or Winlink data, so those can never be sent.

#ifndef TB_INFO_H_
#define TB_INFO_H_

#include <stddef.h>
#include <stdint.h>

namespace TB {

constexpr int    kProtocolVersion = 1;
constexpr size_t kMaxPayload      = 500;   // bytes incl. terminator must stay below the 512 byte BLE attribute limit
constexpr size_t kMaxBeacons      = 5;
constexpr size_t kMaxLoraTypes    = 3;

struct InfoData {
    const char* firmware;
    const char* board;
    const char* callsign;           // callsign of the active beacon profile
    uint32_t    uptimeS;
    bool        batteryPresent;
    float       batteryV;
    int         batteryPct;         // -1 unknown
    bool        hasBatteryMa;       // charge (+) / discharge (-) current, only on boards with a PMU
    int         batteryMa;
    bool        gpsEnabled;
    bool        gpsFix;
    int         satellites;
    float       hdop;
    float       speedKmh;
    int         altitudeM;
    int32_t     gpsAgeS;            // seconds since the last position update, -1 unknown
    int32_t     lastTxAgoS;         // seconds since the last transmission, -1 never
    bool        smartBeacon;
    int         beaconIndex;
    long        loraFreqHz;
    int         sf;
    long        bwHz;
    int         cr;
    int         powerDbm;
    int         storedMessages;
    uint32_t    freeHeap;
};

struct BeaconData {
    const char* callsign;
    const char* symbol;
    const char* overlay;
    const char* comment;
    const char* label;
    bool        smartBeacon;
    int         smartSetting;
    bool        gpsEco;
};

struct LoraData {
    long freqHz;
    int  sf;
    long bwHz;
    int  cr;
    int  powerDbm;
};

struct ConfigData {
    const BeaconData* beacons;
    size_t            beaconCount;
    const char*       path;
    const LoraData*   lora;
    size_t            loraCount;
    bool              dispEco;
    int               dispTimeoutS;
    bool              dispTurn180;
    bool              batSendVoltage;
    bool              batMonitor;
    float             batSleepV;
    bool              simpleMode;
    int               commentEvery;
    int               nonSmartRate;
    int               standingUpdate;
    bool              sendAltitude;
    bool              disableGps;
    bool              cad;
    const char*       btName;
    bool              btBle;
    bool              btKiss;
    bool              telemetry;
};

struct MessageData {
    const char* from;
    const char* text;
};

// Each function writes a NUL-terminated JSON object into [out] (capacity [cap] bytes) and returns its
// length, or 0 when even the smallest form does not fit. They never write outside [out].
size_t buildInfo(const InfoData& data, char* out, size_t cap);
size_t buildConfig(const ConfigData& data, char* out, size_t cap);
// [newestFirst] holds [count] messages; as many as fit are written, whole messages only. [total] is the number
// of messages stored on the tracker.
size_t buildMessages(const MessageData* newestFirst, size_t count, int total, char* out, size_t cap);

}  // namespace TB

#endif
