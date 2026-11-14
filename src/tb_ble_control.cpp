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

#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <SPIFFS.h>
#include <string.h>
#include "configuration.h"
#include "display.h"
#include "tb_ble_control.h"
#include "tb_control.h"
#include "tb_info.h"
#include "tb_lang.h"

#define TB_CONTROL_UUID  "00000014-ba2a-46c9-ae49-01b0961f68bb"
#define TB_RESULT_UUID   "00000015-ba2a-46c9-ae49-01b0961f68bb"
#define TB_COMMAND_MAX   400
#define TB_RESTART_DELAY_MS 1500

extern Configuration Config;
extern bool          displayState;
extern uint32_t      displayTime;

namespace {

    NimBLECharacteristic*   resultChr = nullptr;

    // Command handed over from the Bluetooth task to the main loop.
    portMUX_TYPE            mux = portMUX_INITIALIZER_UNLOCKED;
    char                    pendingCmd[TB_COMMAND_MAX + 2];
    size_t                  pendingLen = 0;
    int                     pendingConn = -1;
    bool                    hasPending = false;
    volatile bool           connectionDirty = false;
    uint32_t                connectionEpoch = 0;     // bumped on every connect/disconnect (under mux)
    uint32_t                pendingEpoch = 0;

    TB::Controller          controller;
    uint32_t                restartAtMs = 0;
    bool                    restartPlanned = false;
    char                    resultBuf[TB::kMaxPayload + 4];

    class ControlCallbacks : public NimBLECharacteristicCallbacks {
        void onWrite(NimBLECharacteristic* characteristic, ble_gap_conn_desc* desc) override {
            NimBLEAttValue value = characteristic->getValue();
            size_t n = value.size();
            portENTER_CRITICAL(&mux);
            if (!hasPending) {
                // an over-long command is cut one byte past the limit, which the rules code refuses
                const size_t keep = n > TB_COMMAND_MAX ? TB_COMMAND_MAX + 1 : n;
                memcpy(pendingCmd, value.data(), keep);
                pendingLen = keep;
                pendingConn = desc ? desc->conn_handle : 0;
                pendingEpoch = connectionEpoch;
                hasPending = true;
            }
            portEXIT_CRITICAL(&mux);
        }
    };

    void readSettings(TB::EditableSettings& s) {
        memset(&s, 0, sizeof(s));
        s.beaconCount = Config.beacons.size() < TB::kMaxBeacons ? Config.beacons.size() : TB::kMaxBeacons;
        for (size_t i = 0; i < s.beaconCount; i++) {
            const Beacon& b = Config.beacons[i];
            strlcpy(s.beacons[i].callsign, b.callsign.c_str(), sizeof(s.beacons[i].callsign));
            strlcpy(s.beacons[i].symbol, b.symbol.c_str(), sizeof(s.beacons[i].symbol));
            strlcpy(s.beacons[i].overlay, b.overlay.c_str(), sizeof(s.beacons[i].overlay));
            strlcpy(s.beacons[i].comment, b.comment.c_str(), sizeof(s.beacons[i].comment));
            strlcpy(s.beacons[i].label, b.profileLabel.c_str(), sizeof(s.beacons[i].label));
            s.beacons[i].smartBeacon = b.smartBeaconActive;
            s.beacons[i].smartSetting = b.smartBeaconSetting;
            s.beacons[i].gpsEco = b.gpsEcoMode;
        }
        strlcpy(s.path, Config.path.c_str(), sizeof(s.path));
        s.loraCount = Config.loraTypes.size() < TB::kMaxLoraTypes ? Config.loraTypes.size() : TB::kMaxLoraTypes;
        for (size_t i = 0; i < s.loraCount; i++) {
            const LoraType& l = Config.loraTypes[i];
            s.lora[i].freqHz = l.frequency;
            s.lora[i].sf = l.spreadingFactor;
            s.lora[i].bwHz = l.signalBandwidth;
            s.lora[i].cr = l.codingRate4;
            s.lora[i].powerDbm = l.power;
        }
        s.dispEco = Config.display.ecoMode;
        s.dispTimeoutS = Config.display.timeout;
        s.dispTurn180 = Config.display.turn180;
        s.batSendVoltage = Config.battery.sendVoltage;
        s.batMonitor = Config.battery.monitorVoltage;
        s.batSleepV = Config.battery.sleepVoltage;
        s.simpleMode = Config.simplifiedTrackerMode;
        s.commentEvery = Config.sendCommentAfterXBeacons;
        s.nonSmartRate = Config.nonSmartBeaconRate;
        s.standingUpdate = Config.standingUpdateTime;
        s.sendAltitude = Config.sendAltitude;
        s.disableGps = Config.disableGPS;
        s.cad = Config.cadActive;
        strlcpy(s.btName, Config.bluetooth.deviceName.c_str(), sizeof(s.btName));
        s.telemetry = Config.telemetry.active;
    }

    bool differs(const char* a, const char* b) {
        return strcmp(a, b) != 0;
    }

    // Writes into [c] only the fields that differ between [staged] and [base] (the settings as they are now). Values
    // that were never touched keep their exact stored form, even when they are longer than the app's buffers, and
    // everything the app may not change (secrets, Mic-E, status text ...) is never touched at all.
    void applySettings(const TB::EditableSettings& staged, const TB::EditableSettings& base, Configuration& c) {
        for (size_t i = 0; i < staged.beaconCount && i < base.beaconCount && i < c.beacons.size(); i++) {
            const TB::EditableBeacon& s = staged.beacons[i];
            const TB::EditableBeacon& o = base.beacons[i];
            Beacon& b = c.beacons[i];
            if (differs(s.callsign, o.callsign)) b.callsign = s.callsign;
            if (differs(s.symbol, o.symbol)) b.symbol = s.symbol;
            if (differs(s.overlay, o.overlay)) b.overlay = s.overlay;
            if (differs(s.comment, o.comment)) b.comment = s.comment;
            if (differs(s.label, o.label)) b.profileLabel = s.label;
            if (s.smartBeacon != o.smartBeacon) b.smartBeaconActive = s.smartBeacon;
            if (s.smartSetting != o.smartSetting) b.smartBeaconSetting = static_cast<byte>(s.smartSetting);
            if (s.gpsEco != o.gpsEco) b.gpsEcoMode = s.gpsEco;
        }
        if (differs(staged.path, base.path)) c.path = staged.path;
        for (size_t i = 0; i < staged.loraCount && i < base.loraCount && i < c.loraTypes.size(); i++) {
            const TB::EditableLora& s = staged.lora[i];
            const TB::EditableLora& o = base.lora[i];
            LoraType& l = c.loraTypes[i];
            if (s.freqHz != o.freqHz) l.frequency = s.freqHz;
            if (s.sf != o.sf) l.spreadingFactor = s.sf;
            if (s.bwHz != o.bwHz) l.signalBandwidth = s.bwHz;
            if (s.cr != o.cr) l.codingRate4 = s.cr;
            if (s.powerDbm != o.powerDbm) l.power = s.powerDbm;
        }
        if (staged.dispEco != base.dispEco) c.display.ecoMode = staged.dispEco;
        if (staged.dispTimeoutS != base.dispTimeoutS) c.display.timeout = staged.dispTimeoutS;
        if (staged.dispTurn180 != base.dispTurn180) c.display.turn180 = staged.dispTurn180;
        if (staged.batSendVoltage != base.batSendVoltage) c.battery.sendVoltage = staged.batSendVoltage;
        if (staged.batMonitor != base.batMonitor) c.battery.monitorVoltage = staged.batMonitor;
        if (staged.batSleepV != base.batSleepV) c.battery.sleepVoltage = staged.batSleepV;
        if (staged.simpleMode != base.simpleMode) c.simplifiedTrackerMode = staged.simpleMode;
        if (staged.commentEvery != base.commentEvery) c.sendCommentAfterXBeacons = staged.commentEvery;
        if (staged.nonSmartRate != base.nonSmartRate) c.nonSmartBeaconRate = staged.nonSmartRate;
        if (staged.standingUpdate != base.standingUpdate) c.standingUpdateTime = staged.standingUpdate;
        if (staged.sendAltitude != base.sendAltitude) c.sendAltitude = staged.sendAltitude;
        if (staged.disableGps != base.disableGps) c.disableGPS = staged.disableGps;
        if (staged.cad != base.cad) c.cadActive = staged.cad;
        if (differs(staged.btName, base.btName)) c.bluetooth.deviceName = staged.btName;
        if (staged.telemetry != base.telemetry) c.telemetry.active = staged.telemetry;
    }

    const char* kSettingsFile = "/tracker_conf.json";

    String readTextFile(const char* path) {
        File f = SPIFFS.open(path, "r");
        if (!f) return "";
        String text = f.readString();
        f.close();
        return text;
    }

    void restoreTextFile(const char* path, const String& text) {
        if (text.length() == 0) return;
        File f = SPIFFS.open(path, "w");
        if (!f) return;
        f.print(text);
        f.close();
    }

    // Saves [c] and checks that the file can be read back; if not, the previous file is put back.
    bool saveChecked(Configuration& c) {
        const String previous = readTextFile(kSettingsFile);
        if (!c.writeFile()) {
            restoreTextFile(kSettingsFile, previous);
            return false;
        }
        const String written = readTextFile(kSettingsFile);
        JsonDocument check;
        if (written.length() < 2 || deserializeJson(check, written)) {
            restoreTextFile(kSettingsFile, previous);
            return false;
        }
        return true;
    }

    void publishResult(const TB::CommandResult& r) {
        const size_t n = TB::buildCommandResult(r, resultBuf, TB::kMaxPayload);
        if (resultChr && n > 0) resultChr->setValue(reinterpret_cast<const uint8_t*>(resultBuf), n);
    }

    void handleCommand(const char* cmd, size_t len, int conn) {
        const uint32_t now = millis();
        static TB::EditableSettings current;    // static: keeps the loop task's stack small
        readSettings(current);
        TB::Action action;
        TB::CommandResult result = controller.handle(cmd, len, conn, now, esp_random(), current, action);

        if (action.showPin) {
            // wake the screen so the code can be read
            displayState = true;
            displayTime = now;
            displayToggle(true);
        }
        if (action.commit) {
            static TB::EditableSettings base;
            readSettings(base);    // the settings as they are right now
            Configuration copy = Config;
            applySettings(controller.staged(), base, copy);
            #ifndef HAS_BT_CLASSIC
                // The app talks over Bluetooth, so after a set-up from the app it must stay on at the next start too.
                copy.bluetooth.active = true;
                copy.bluetooth.useBLE = true;
            #endif
            if (saveChecked(copy)) {
                restartPlanned = true;
                restartAtMs = now + TB_RESTART_DELAY_MS;
            } else {
                result.ok = false;
                result.err = "save_failed";
                result.restart = false;
            }
        }
        publishResult(result);
    }

}

namespace TB_BLE_CONTROL {

    void setup(NimBLEService* service) {
        if (!service) return;
        NimBLECharacteristic* control = service->createCharacteristic(TB_CONTROL_UUID, NIMBLE_PROPERTY::WRITE);
        resultChr = service->createCharacteristic(TB_RESULT_UUID, NIMBLE_PROPERTY::READ);
        control->setCallbacks(new ControlCallbacks());
        static const uint8_t zeros[TB::kMaxPayload] = {0};
        resultChr->setValue(zeros, sizeof(zeros));    // reserve the size once (see tb_ble_info.cpp)
        TB::CommandResult idle = {0, false, nullptr, "idle", nullptr, -1, 0, false};
        publishResult(idle);
    }

    void connectionChanged() {
        portENTER_CRITICAL(&mux);
        connectionEpoch++;      // a command written before this belongs to the old connection
        hasPending = false;
        portEXIT_CRITICAL(&mux);
        connectionDirty = true;
    }

    void loop() {
        const uint32_t now = millis();
        if (connectionDirty) {
            connectionDirty = false;
            controller.onDisconnect(-1);    // a new or lost connection never inherits a permission
        }
        controller.tick(now);
        if (restartPlanned && static_cast<int32_t>(now - restartAtMs) >= 0) ESP.restart();

        static char cmd[TB_COMMAND_MAX + 2];
        size_t len = 0;
        int conn = -1;
        bool have = false;
        portENTER_CRITICAL(&mux);
        if (hasPending) {
            if (pendingEpoch == connectionEpoch) {
                memcpy(cmd, pendingCmd, pendingLen);
                len = pendingLen;
                conn = pendingConn;
                have = true;
            }
            hasPending = false;
        }
        portEXIT_CRITICAL(&mux);
        if (have) handleCommand(cmd, len, conn);
    }

    bool pinActive() {
        return controller.pinActive(millis());
    }

    bool drawPinScreen() {
        const uint32_t now = millis();
        if (!controller.pinActive(now)) return false;
        displayTime = now;                 // keep the screen on (eco mode would switch it off after a few seconds)
        if (!displayState) {
            displayState = true;
            displayToggle(true);
        }
        const int seconds = static_cast<int>((controller.pinRemainingMs(now) + 999u) / 1000u);
        displayShow("TRACKBRIDGE", TB_T("Pairing code:", "Eslestirme kodu:"), controller.pin(),
                    TB_T("Type it in the app", "Uygulamaya yazin"), String(seconds) + " s", "", 0);
        return true;
    }

}
