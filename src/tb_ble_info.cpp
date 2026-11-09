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

#include <NimBLEDevice.h>
#include <SPIFFS.h>
#include <TinyGPS++.h>
#include <deque>
#include <string.h>
#include <vector>
#include "configuration.h"
#include "battery_utils.h"
#include "board_pinout.h"
#include "tb_ble_info.h"
#include "tb_info.h"
#include "msg_utils.h"

// Same UUID family as the KISS service, own range. All characteristics are READ only.
#define TB_SERVICE_UUID   "00000010-ba2a-46c9-ae49-01b0961f68bb"
#define TB_INFO_UUID      "00000011-ba2a-46c9-ae49-01b0961f68bb"
#define TB_CONFIG_UUID    "00000012-ba2a-46c9-ae49-01b0961f68bb"
#define TB_MESSAGES_UUID  "00000013-ba2a-46c9-ae49-01b0961f68bb"

#define TB_FIRMWARE_VERSION  "4.0.1-tb1"
#define TB_REFRESH_MS        3000
#define TB_MESSAGES_FILE     "/aprsMessages.txt"
#define TB_MESSAGES_TAIL     1600     // bytes read from the end of the file
#define TB_MESSAGES_MAX      8

extern Configuration    Config;
extern TinyGPSPlus      gps;
extern bool             gpsIsActive;
extern bool             bluetoothConnected;
extern Beacon*          currentBeacon;
extern uint8_t          myBeaconsIndex;
extern LoraType*        currentLoRaType;
extern bool             smartBeaconActive;
extern uint32_t         lastTxTime;
extern String           batteryVoltage;
extern bool             batteryConnected;
#if defined(HAS_AXP192) || defined(HAS_AXP2101)
    extern String       batteryChargeDischargeCurrent;
#endif

namespace {

    NimBLECharacteristic*   infoChr         = nullptr;
    NimBLECharacteristic*   configChr       = nullptr;
    NimBLECharacteristic*   messagesChr     = nullptr;

    uint32_t                lastRefreshMs   = 0;
    bool                    refreshedOnce   = false;
    int                     lastMsgCount    = -1;

    char                    infoBuf[TB::kMaxPayload + 4];
    char                    configBuf[TB::kMaxPayload + 4];
    char                    messagesBuf[TB::kMaxPayload + 4];
    char                    configLast[TB::kMaxPayload + 4] = "";

    const char* boardName() {
        #ifdef HELTEC_WIRELESS_TRACKER
            return "heltec_wireless_tracker";
        #elif defined(ARDUINO_BOARD)
            return ARDUINO_BOARD;
        #else
            return "esp32";
        #endif
    }

    int batteryPercent(float volts) {
        int pct = (int)(((volts / BATTERY_CELLS) - 3.0f) / (4.2f - 3.0f) * 100.0f);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        return pct;
    }

    void publish(NimBLECharacteristic* chr, const char* buf, size_t len) {
        if (chr && len > 0) chr->setValue((const uint8_t*)buf, len);
    }

    void publishInfo() {
        TB::InfoData d = {};
        d.firmware = TB_FIRMWARE_VERSION;
        d.board = boardName();
        d.callsign = currentBeacon ? currentBeacon->callsign.c_str() : "";
        d.uptimeS = millis() / 1000;

        const float volts = batteryVoltage.toFloat();    // measured by BATTERY_Utils::monitor(); no extra ADC reads here
        d.batteryPresent = batteryConnected && volts > 1.5f;
        d.batteryV = volts;
        d.batteryPct = d.batteryPresent ? batteryPercent(volts) : -1;
        #if defined(HAS_AXP192) || defined(HAS_AXP2101)
            d.hasBatteryMa = d.batteryPresent && batteryChargeDischargeCurrent.length() > 0;
            d.batteryMa = batteryChargeDischargeCurrent.toInt();
        #endif

        d.gpsEnabled = gpsIsActive && !Config.disableGPS;
        d.gpsFix = gps.location.isValid();
        d.satellites = gps.satellites.isValid() ? (int)gps.satellites.value() : 0;
        d.hdop = gps.hdop.isValid() ? (float)gps.hdop.hdop() : 0.0f;
        d.speedKmh = gps.speed.isValid() ? (float)gps.speed.kmph() : 0.0f;
        d.altitudeM = gps.altitude.isValid() ? (int)gps.altitude.meters() : 0;
        const uint32_t gpsAgeMs = gps.location.age();
        d.gpsAgeS = (gpsAgeMs == 0xFFFFFFFFu || !gps.location.isValid()) ? -1 : (int32_t)(gpsAgeMs / 1000);
        d.lastTxAgoS = lastTxTime == 0 ? -1 : (int32_t)((millis() - lastTxTime) / 1000);
        d.smartBeacon = smartBeaconActive;
        d.beaconIndex = myBeaconsIndex;
        if (currentLoRaType) {
            d.loraFreqHz = currentLoRaType->frequency;
            d.sf = currentLoRaType->spreadingFactor;
            d.bwHz = currentLoRaType->signalBandwidth;
            d.cr = currentLoRaType->codingRate4;
            d.powerDbm = currentLoRaType->power;
        }
        d.storedMessages = MSG_Utils::getNumAPRSMessages();
        d.freeHeap = ESP.getFreeHeap();
        publish(infoChr, infoBuf, TB::buildInfo(d, infoBuf, TB::kMaxPayload));
    }

    // Only fields on the whitelist in tb_info.h are copied. Passwords, e-mail and Winlink data are not.
    void publishConfig() {
        TB::BeaconData beacons[TB::kMaxBeacons] = {};
        size_t nb = Config.beacons.size() < TB::kMaxBeacons ? Config.beacons.size() : TB::kMaxBeacons;
        for (size_t i = 0; i < nb; i++) {
            const Beacon& b = Config.beacons[i];
            beacons[i].callsign = b.callsign.c_str();
            beacons[i].symbol = b.symbol.c_str();
            beacons[i].overlay = b.overlay.c_str();
            beacons[i].comment = b.comment.c_str();
            beacons[i].label = b.profileLabel.c_str();
            beacons[i].smartBeacon = b.smartBeaconActive;
            beacons[i].smartSetting = b.smartBeaconSetting;
            beacons[i].gpsEco = b.gpsEcoMode;
        }
        TB::LoraData lora[TB::kMaxLoraTypes] = {};
        size_t nl = Config.loraTypes.size() < TB::kMaxLoraTypes ? Config.loraTypes.size() : TB::kMaxLoraTypes;
        for (size_t i = 0; i < nl; i++) {
            const LoraType& l = Config.loraTypes[i];
            lora[i].freqHz = l.frequency;
            lora[i].sf = l.spreadingFactor;
            lora[i].bwHz = l.signalBandwidth;
            lora[i].cr = l.codingRate4;
            lora[i].powerDbm = l.power;
        }
        TB::ConfigData c = {};
        c.beacons = beacons;
        c.beaconCount = nb;
        c.path = Config.path.c_str();
        c.lora = lora;
        c.loraCount = nl;
        c.dispEco = Config.display.ecoMode;
        c.dispTimeoutS = Config.display.timeout;
        c.dispTurn180 = Config.display.turn180;
        c.batSendVoltage = Config.battery.sendVoltage;
        c.batMonitor = Config.battery.monitorVoltage;
        c.batSleepV = Config.battery.sleepVoltage;
        c.simpleMode = Config.simplifiedTrackerMode;
        c.commentEvery = Config.sendCommentAfterXBeacons;
        c.nonSmartRate = Config.nonSmartBeaconRate;
        c.standingUpdate = Config.standingUpdateTime;
        c.sendAltitude = Config.sendAltitude;
        c.disableGps = Config.disableGPS;
        c.cad = Config.cadActive;
        c.btName = Config.bluetooth.deviceName.c_str();
        c.btBle = Config.bluetooth.useBLE;
        c.btKiss = Config.bluetooth.useKISS;
        c.telemetry = Config.telemetry.active;
        const size_t n = TB::buildConfig(c, configBuf, TB::kMaxPayload);
        if (n > 0 && strcmp(configBuf, configLast) != 0) {    // only touch the characteristic when something changed
            publish(configChr, configBuf, n);
            strcpy(configLast, configBuf);
        }
    }

    // Newest messages stored on the tracker ("STATION,text" lines). Reads only the end of the file.
    void publishMessages() {
        // Keep only the newest TB_MESSAGES_MAX valid lines of the tail, whatever their length.
        std::deque<String> newestLast;
        File f = SPIFFS.open(TB_MESSAGES_FILE, "r");
        if (f) {
            f.setTimeout(0);    // a last line without '\n' (power loss while saving) must not block the main loop
            const size_t size = f.size();
            const size_t start = size > TB_MESSAGES_TAIL ? size - TB_MESSAGES_TAIL : 0;
            if (start > 0 && f.seek(start)) f.readStringUntil('\n');    // drop the cut first line
            while (f.available()) {
                String line = f.readStringUntil('\n');
                line.trim();
                if (line.length() == 0 || line.indexOf(',') < 0) continue;
                newestLast.push_back(line);
                if (newestLast.size() > TB_MESSAGES_MAX) newestLast.pop_front();
            }
            f.close();
        }
        std::vector<String> from, text;
        for (int i = (int)newestLast.size() - 1; i >= 0; i--) {    // newest first
            const String& line = newestLast[i];
            const int comma = line.indexOf(',');
            from.push_back(line.substring(0, comma));
            text.push_back(line.substring(comma + 1));
        }
        TB::MessageData items[TB_MESSAGES_MAX] = {};
        for (size_t i = 0; i < from.size(); i++) {
            items[i].from = from[i].c_str();
            items[i].text = text[i].c_str();
        }
        publish(messagesChr, messagesBuf,
                TB::buildMessages(items, from.size(), MSG_Utils::getNumAPRSMessages(), messagesBuf, TB::kMaxPayload));
    }

}

namespace TB_BLE {

    void setup(NimBLEServer* server) {
        if (!server) return;
        NimBLEService* service = server->createService(TB_SERVICE_UUID);
        if (!service) return;
        infoChr     = service->createCharacteristic(TB_INFO_UUID, NIMBLE_PROPERTY::READ);
        configChr   = service->createCharacteristic(TB_CONFIG_UUID, NIMBLE_PROPERTY::READ);
        messagesChr = service->createCharacteristic(TB_MESSAGES_UUID, NIMBLE_PROPERTY::READ);
        service->start();
        // Reserve the full size once, so later updates never reallocate while a phone is reading.
        static const uint8_t zeros[TB::kMaxPayload] = {0};
        infoChr->setValue(zeros, sizeof(zeros));
        configChr->setValue(zeros, sizeof(zeros));
        messagesChr->setValue(zeros, sizeof(zeros));
        publishInfo();
        publishConfig();
        publishMessages();
        lastMsgCount = MSG_Utils::getNumAPRSMessages();
    }

    void loop() {
        if (!infoChr || !bluetoothConnected) return;
        const uint32_t now = millis();
        if (!refreshedOnce || now - lastRefreshMs >= TB_REFRESH_MS) {
            refreshedOnce = true;
            lastRefreshMs = now;
            publishInfo();
            publishConfig();    // cheap, and the active profile / frequency can change at runtime
        }
        const int count = MSG_Utils::getNumAPRSMessages();
        if (count != lastMsgCount) {
            lastMsgCount = count;
            publishMessages();
        }
    }

}
