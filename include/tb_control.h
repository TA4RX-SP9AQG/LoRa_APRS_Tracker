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

// Changing the tracker's settings from the TrackBridge app, as plain C++ without Arduino so it can be tested on a PC.
//
// Security model: anybody within Bluetooth range can write to the control characteristic, so nothing is accepted
// before the user proves to be next to the tracker: the phone asks for a PIN ("pin"), the tracker shows a random
// 6-digit code on its own screen, the user types it into the app ("auth"). A PIN is valid for 60 s and for one
// successful use, three wrong tries lock everything for 60 s, and the permission ends when the connection drops or
// after 5 minutes without activity. Only fields on the allow-list in applyPatch() can be changed, every value is
// checked, and a change only becomes real with "commit" (all-or-nothing, the tracker restarts). Passwords, e-mail and
// Winlink data cannot be touched, and Bluetooth cannot be switched off from here (that would lock the app out).

#ifndef TB_CONTROL_H_
#define TB_CONTROL_H_

#include <stddef.h>
#include <stdint.h>

#include "tb_info.h"

// The radio chip decides which values the tracker can really use. A value the chip refuses makes the tracker stop at
// start-up, so the limits below are those of the chip on the board (the PC tests use the SX1262 limits).
#if __has_include("board_pinout.h")
#include "board_pinout.h"
#endif

namespace TB {

#if defined(HAS_SX1278)
constexpr long kFreqMinHz = 137000000L, kFreqMaxHz = 525000000L;
constexpr int  kPowerMaxDbm = 20, kSfMin = 6;
#elif defined(HAS_SX1276)
constexpr long kFreqMinHz = 862000000L, kFreqMaxHz = 960000000L;
constexpr int  kPowerMaxDbm = 20, kSfMin = 6;
#elif defined(HAS_SX1268)
constexpr long kFreqMinHz = 410000000L, kFreqMaxHz = 810000000L;
constexpr int  kPowerMaxDbm = 22, kSfMin = 5;
#else    // SX1262
constexpr long kFreqMinHz = 150000000L, kFreqMaxHz = 960000000L;
constexpr int  kPowerMaxDbm = 22, kSfMin = 5;
#endif

constexpr size_t kCallsignLen = 16;
constexpr size_t kCommentLen = 64;
constexpr size_t kLabelLen = 24;
constexpr size_t kPathLen = 48;
constexpr size_t kBtNameLen = 28;

constexpr uint32_t kPinValidMs = 60u * 1000u;
constexpr uint32_t kPinRequestGapMs = 10u * 1000u;
constexpr uint32_t kPinRequestWindowMs = 5u * 60u * 1000u;    // at most kPinRequestsPerWindow PINs in this time
constexpr int      kPinRequestsPerWindow = 3;
constexpr uint32_t kLockoutMs = 60u * 1000u;               // first lockout; each further one doubles
constexpr uint32_t kMaxLockoutMs = 30u * 60u * 1000u;
constexpr uint32_t kSessionMs = 5u * 60u * 1000u;
constexpr int kPinTries = 3;

struct EditableBeacon {
    char callsign[kCallsignLen];
    char symbol[2];
    char overlay[2];
    char comment[kCommentLen];
    char label[kLabelLen];
    bool smartBeacon;
    int  smartSetting;
    bool gpsEco;
};

struct EditableLora {
    long freqHz;
    int  sf;
    long bwHz;
    int  cr;
    int  powerDbm;
};

// Everything the app may change. Copied from / written back to the tracker's configuration by the Arduino glue code.
struct EditableSettings {
    EditableBeacon beacons[kMaxBeacons];
    size_t         beaconCount;
    char           path[kPathLen];
    EditableLora   lora[kMaxLoraTypes];
    size_t         loraCount;
    bool           dispEco;
    int            dispTimeoutS;
    bool           dispTurn180;
    bool           batSendVoltage;
    bool           batMonitor;
    float          batSleepV;
    bool           simpleMode;
    int            commentEvery;
    int            nonSmartRate;
    int            standingUpdate;
    bool           sendAltitude;
    bool           disableGps;
    bool           cad;
    char           btName[kBtNameLen];
    bool           telemetry;
};

// What the glue code has to do after a command.
struct Action {
    bool showPin;       // display Controller::pin() on the tracker's screen
    bool commit;        // write Controller::staged() to the settings file and restart
};

class Controller {
public:
    Controller();

    // Handles one command (JSON text). Always returns a result to hand back to the phone.
    CommandResult handle(const char* json, size_t len, int connId, uint32_t nowMs, uint32_t random,
                         const EditableSettings& current, Action& action);

    // Connection closed: the permission and unsaved changes of that connection are gone.
    void onDisconnect(int connId);

    // Call regularly: ends expired PINs and sessions.
    void tick(uint32_t nowMs);

    bool           pinActive(uint32_t nowMs) const;
    const char*    pin() const { return pin_; }
    uint32_t       pinRemainingMs(uint32_t nowMs) const;
    const EditableSettings& staged() const { return staged_; }
    bool           hasStaged() const { return hasStaged_; }

private:
    CommandResult fail(long id, const char* op, const char* err);
    bool authed(int connId, uint32_t nowMs) const;

    char             pin_[8];
    int              pinConn_;
    uint32_t         requestTimes_[kPinRequestsPerWindow];
    int              requestCount_;
    int              wrongStreak_;      // wrong tries since the last success, over all PINs
    int              lockLevel_;        // lockouts since the last success
    bool             pinSet_;
    uint32_t         pinExpiresMs_;
    uint32_t         lastPinRequestMs_;
    bool             anyPinRequested_;
    int              triesLeft_;
    bool             locked_;
    uint32_t         lockedUntilMs_;
    int              authedConn_;
    uint32_t         sessionUntilMs_;
    bool             hasStaged_;
    EditableSettings staged_;
    char             field_[24];
    char             op_[12];
};

// Checks and applies one "set" patch (a JSON object) to [settings]. Returns false and names the offending key in
// [badField] when anything is wrong; [settings] is only changed when everything is fine.
struct PatchResult {
    bool ok;
    char field[24];
};
PatchResult validatePatchText(const char* json, size_t len, EditableSettings& settings);

// Individual value checks (also used by the tests).
bool validCallsign(const char* s);
bool validPath(const char* s);
bool validComment(const char* s);
bool validBtName(const char* s);

}  // namespace TB

#endif
