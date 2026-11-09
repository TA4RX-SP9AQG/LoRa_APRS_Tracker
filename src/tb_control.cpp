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

#include "tb_control.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

namespace TB {
namespace {

constexpr size_t kMaxCommandLen = 400;
constexpr size_t kCommentMax = 60;   // characters; the buffer is larger
constexpr size_t kLabelMax = 20;

bool isUpperOrDigit(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool isPrintable(char c) {
    return c >= 0x20 && c <= 0x7E;
}

// "SP9AQG-9": 3 to 6 letters/digits, optionally "-" and an SSID of 0 to 15.
bool callsignWithOptionalSsid(const char* s, size_t len, bool allowTrailingStar) {
    (void)allowTrailingStar;
    size_t base = 0;
    while (base < len && s[base] != '-') {
        if (!isUpperOrDigit(s[base])) return false;
        base++;
    }
    if (base < 3 || base > 6) return false;
    if (base == len) return true;
    const size_t ssidLen = len - base - 1;    // after the '-'
    if (ssidLen < 1 || ssidLen > 2) return false;
    if (ssidLen == 2 && s[base + 1] == '0') return false;    // "-05" is not how an SSID is written
    int value = 0;
    for (size_t i = base + 1; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        value = value * 10 + (s[i] - '0');
    }
    return value <= 15;
}

void copyText(char* dst, size_t cap, const char* src) {
    if (cap == 0) return;
    size_t i = 0;
    for (; src && src[i] && i + 1 < cap; i++) dst[i] = src[i];
    dst[i] = '\0';
}

void setField(char* out, const char* section, const char* key) {
    if (section && section[0]) {
        snprintf(out, 24, "%s.%s", section, key);
    } else {
        snprintf(out, 24, "%s", key);
    }
}

bool readBool(JsonVariantConst v, bool& out) {
    if (v.is<bool>()) {
        out = v.as<bool>();
        return true;
    }
    if (v.is<int>()) {
        const int n = v.as<int>();
        if (n == 0 || n == 1) {
            out = (n == 1);
            return true;
        }
    }
    return false;
}

bool readInt(JsonVariantConst v, long lo, long hi, long& out) {
    if (!v.is<long>()) return false;
    const long n = v.as<long>();
    if (n < lo || n > hi) return false;
    out = n;
    return true;
}

bool readIntAsInt(JsonVariantConst v, long lo, long hi, int& out) {
    long n = 0;
    if (!readInt(v, lo, hi, n)) return false;
    out = static_cast<int>(n);
    return true;
}

bool validBandwidth(long bw) {
    static const long allowed[] = {7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
        if (allowed[i] == bw) return true;
    }
    return false;
}

bool applyBeacon(JsonObjectConst o, EditableSettings& s, char* bad) {
    long index = 0;
    if (!readInt(o["i"], 0, static_cast<long>(s.beaconCount) - 1, index)) {
        setField(bad, "b", "i");
        return false;
    }
    EditableBeacon& b = s.beacons[index];
    for (JsonPairConst kv : o) {
        const char* k = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(k, "i")) continue;
        if (!strcmp(k, "cs")) {
            if (!v.is<const char*>() || !validCallsign(v.as<const char*>())) { setField(bad, "b", k); return false; }
            copyText(b.callsign, sizeof(b.callsign), v.as<const char*>());
        } else if (!strcmp(k, "sym")) {
            const char* t = v.is<const char*>() ? v.as<const char*>() : nullptr;
            if (!t || strlen(t) != 1 || t[0] < 0x21 || t[0] > 0x7E) { setField(bad, "b", k); return false; }
            copyText(b.symbol, sizeof(b.symbol), t);
        } else if (!strcmp(k, "ov")) {
            const char* t = v.is<const char*>() ? v.as<const char*>() : nullptr;
            if (!t || strlen(t) != 1 || !(t[0] == '/' || t[0] == '\\' || isUpperOrDigit(t[0]))) { setField(bad, "b", k); return false; }
            copyText(b.overlay, sizeof(b.overlay), t);
        } else if (!strcmp(k, "cm")) {
            if (!v.is<const char*>() || !validComment(v.as<const char*>())) { setField(bad, "b", k); return false; }
            copyText(b.comment, sizeof(b.comment), v.as<const char*>());
        } else if (!strcmp(k, "lbl")) {
            const char* t = v.is<const char*>() ? v.as<const char*>() : nullptr;
            bool ok = t && strlen(t) <= kLabelMax;
            for (size_t i = 0; ok && t[i]; i++) ok = isPrintable(t[i]);
            if (!ok) { setField(bad, "b", k); return false; }
            copyText(b.label, sizeof(b.label), t);
        } else if (!strcmp(k, "sb")) {
            if (!readBool(v, b.smartBeacon)) { setField(bad, "b", k); return false; }
        } else if (!strcmp(k, "sbs")) {
            if (!readIntAsInt(v, 0, 2, b.smartSetting)) { setField(bad, "b", k); return false; }
        } else if (!strcmp(k, "eco")) {
            if (!readBool(v, b.gpsEco)) { setField(bad, "b", k); return false; }
        } else {
            setField(bad, "b", k);
            return false;
        }
    }
    return true;
}

bool applyLora(JsonObjectConst o, EditableSettings& s, char* bad) {
    long index = 0;
    if (!readInt(o["i"], 0, static_cast<long>(s.loraCount) - 1, index)) {
        setField(bad, "l", "i");
        return false;
    }
    EditableLora& l = s.lora[index];
    for (JsonPairConst kv : o) {
        const char* k = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(k, "i")) continue;
        if (!strcmp(k, "f")) {
            if (!readInt(v, kFreqMinHz, kFreqMaxHz, l.freqHz)) { setField(bad, "l", k); return false; }
        } else if (!strcmp(k, "sf")) {
            if (!readIntAsInt(v, kSfMin, 12, l.sf)) { setField(bad, "l", k); return false; }
        } else if (!strcmp(k, "bw")) {
            long bw = 0;
            if (!readInt(v, 7800, 500000, bw) || !validBandwidth(bw)) { setField(bad, "l", k); return false; }
            l.bwHz = bw;
        } else if (!strcmp(k, "cr")) {
            if (!readIntAsInt(v, 5, 8, l.cr)) { setField(bad, "l", k); return false; }
        } else if (!strcmp(k, "pw")) {
            if (!readIntAsInt(v, 0, kPowerMaxDbm, l.powerDbm)) { setField(bad, "l", k); return false; }
        } else {
            setField(bad, "l", k);
            return false;
        }
    }
    return true;
}

bool applyDisplay(JsonObjectConst o, EditableSettings& s, char* bad) {
    for (JsonPairConst kv : o) {
        const char* k = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(k, "eco")) { if (!readBool(v, s.dispEco)) { setField(bad, "disp", k); return false; } }
        else if (!strcmp(k, "to")) { if (!readIntAsInt(v, 1, 255, s.dispTimeoutS)) { setField(bad, "disp", k); return false; } }
        else if (!strcmp(k, "t180")) { if (!readBool(v, s.dispTurn180)) { setField(bad, "disp", k); return false; } }
        else { setField(bad, "disp", k); return false; }
    }
    return true;
}

bool applyBattery(JsonObjectConst o, EditableSettings& s, char* bad) {
    for (JsonPairConst kv : o) {
        const char* k = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(k, "sv")) { if (!readBool(v, s.batSendVoltage)) { setField(bad, "bat", k); return false; } }
        else if (!strcmp(k, "mon")) { if (!readBool(v, s.batMonitor)) { setField(bad, "bat", k); return false; } }
        else if (!strcmp(k, "sleep")) {
            if (!v.is<float>()) { setField(bad, "bat", k); return false; }
            const float f = v.as<float>();
            if (!(f >= 2.5f && f <= 4.0f)) { setField(bad, "bat", k); return false; }
            s.batSleepV = f;
        } else { setField(bad, "bat", k); return false; }
    }
    return true;
}

bool applyOther(JsonObjectConst o, EditableSettings& s, char* bad) {
    for (JsonPairConst kv : o) {
        const char* k = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(k, "simple")) { if (!readBool(v, s.simpleMode)) { setField(bad, "o", k); return false; } }
        else if (!strcmp(k, "cmt")) { if (!readIntAsInt(v, 1, 255, s.commentEvery)) { setField(bad, "o", k); return false; } }
        else if (!strcmp(k, "rate")) { if (!readIntAsInt(v, 1, 255, s.nonSmartRate)) { setField(bad, "o", k); return false; } }
        else if (!strcmp(k, "stand")) { if (!readIntAsInt(v, 1, 255, s.standingUpdate)) { setField(bad, "o", k); return false; } }
        else if (!strcmp(k, "alt")) { if (!readBool(v, s.sendAltitude)) { setField(bad, "o", k); return false; } }
        else if (!strcmp(k, "nogps")) { if (!readBool(v, s.disableGps)) { setField(bad, "o", k); return false; } }
        else if (!strcmp(k, "cad")) { if (!readBool(v, s.cad)) { setField(bad, "o", k); return false; } }
        else { setField(bad, "o", k); return false; }
    }
    return true;
}

// Only the name; whether Bluetooth is on and which protocol it speaks cannot be changed from the app.
bool applyBluetooth(JsonObjectConst o, EditableSettings& s, char* bad) {
    for (JsonPairConst kv : o) {
        const char* k = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(k, "name")) {
            if (!v.is<const char*>() || !validBtName(v.as<const char*>())) { setField(bad, "bt", k); return false; }
            copyText(s.btName, sizeof(s.btName), v.as<const char*>());
        } else { setField(bad, "bt", k); return false; }
    }
    return true;
}

// Applies every section of a "set" command to [s]. Unknown keys are errors, so nothing outside the allow-list
// (passwords, e-mail, Winlink, Bluetooth switches ...) can ever be reached.
bool applyAll(JsonObjectConst cmd, EditableSettings& s, char* bad, bool& any) {
    any = false;
    for (JsonPairConst kv : cmd) {
        const char* k = kv.key().c_str();
        JsonVariantConst v = kv.value();
        if (!strcmp(k, "id") || !strcmp(k, "op")) continue;
        any = true;
        if (!strcmp(k, "b")) {
            if (!v.is<JsonObjectConst>() || !applyBeacon(v.as<JsonObjectConst>(), s, bad)) { if (!bad[0]) setField(bad, nullptr, k); return false; }
        } else if (!strcmp(k, "l")) {
            if (!v.is<JsonObjectConst>() || !applyLora(v.as<JsonObjectConst>(), s, bad)) { if (!bad[0]) setField(bad, nullptr, k); return false; }
        } else if (!strcmp(k, "path")) {
            if (!v.is<const char*>() || !validPath(v.as<const char*>())) { setField(bad, nullptr, k); return false; }
            copyText(s.path, sizeof(s.path), v.as<const char*>());
        } else if (!strcmp(k, "disp")) {
            if (!v.is<JsonObjectConst>() || !applyDisplay(v.as<JsonObjectConst>(), s, bad)) { if (!bad[0]) setField(bad, nullptr, k); return false; }
        } else if (!strcmp(k, "bat")) {
            if (!v.is<JsonObjectConst>() || !applyBattery(v.as<JsonObjectConst>(), s, bad)) { if (!bad[0]) setField(bad, nullptr, k); return false; }
        } else if (!strcmp(k, "o")) {
            if (!v.is<JsonObjectConst>() || !applyOther(v.as<JsonObjectConst>(), s, bad)) { if (!bad[0]) setField(bad, nullptr, k); return false; }
        } else if (!strcmp(k, "bt")) {
            if (!v.is<JsonObjectConst>() || !applyBluetooth(v.as<JsonObjectConst>(), s, bad)) { if (!bad[0]) setField(bad, nullptr, k); return false; }
        } else if (!strcmp(k, "tel")) {
            if (!readBool(v, s.telemetry)) { setField(bad, nullptr, k); return false; }
        } else {
            setField(bad, nullptr, k);
            return false;
        }
    }
    return true;
}

int32_t until(uint32_t deadline, uint32_t now) {
    return static_cast<int32_t>(deadline - now);   // wrap-safe
}

int secondsCeil(uint32_t ms) {
    return static_cast<int>((ms + 999u) / 1000u);
}

}  // namespace

bool validCallsign(const char* s) {
    if (!s) return false;
    const size_t len = strlen(s);
    return len >= 3 && len <= 9 && callsignWithOptionalSsid(s, len, false);
}

bool validPath(const char* s) {
    if (!s) return false;
    const size_t len = strlen(s);
    if (len == 0) return true;
    if (len > 40) return false;
    int hops = 0;
    size_t start = 0;
    while (start <= len) {
        size_t end = start;
        while (end < len && s[end] != ',') end++;
        if (end == start) return false;                               // empty hop
        if (!callsignWithOptionalSsid(s + start, end - start, false)) return false;
        hops++;
        if (hops > 3) return false;
        if (end == len) break;
        start = end + 1;
    }
    return true;
}

bool validComment(const char* s) {
    if (!s) return false;
    const size_t len = strlen(s);
    if (len > kCommentMax) return false;
    for (size_t i = 0; i < len; i++) {
        if (!isPrintable(s[i]) || s[i] == '|' || s[i] == '~') return false;
    }
    return true;
}

bool validBtName(const char* s) {
    if (!s) return false;
    const size_t len = strlen(s);
    if (len < 1 || len > 24) return false;
    for (size_t i = 0; i < len; i++) {
        const char c = s[i];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

PatchResult validatePatchText(const char* json, size_t len, EditableSettings& settings) {
    PatchResult r;
    r.ok = false;
    r.field[0] = '\0';
    if (!json || len == 0 || len > kMaxCommandLen) return r;
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return r;
    if (!doc.is<JsonObject>()) return r;
    EditableSettings copy = settings;
    bool any = false;
    if (!applyAll(doc.as<JsonObjectConst>(), copy, r.field, any) || !any) return r;
    settings = copy;
    r.ok = true;
    return r;
}

Controller::Controller()
    : pinConn_(-1), requestCount_(0), wrongStreak_(0), lockLevel_(0), pinSet_(false), pinExpiresMs_(0), lastPinRequestMs_(0), anyPinRequested_(false), triesLeft_(0), locked_(false),
      lockedUntilMs_(0), authedConn_(-1), sessionUntilMs_(0), hasStaged_(false) {
    pin_[0] = '\0';
    field_[0] = '\0';
    op_[0] = '\0';
    for (int i = 0; i < kPinRequestsPerWindow; i++) requestTimes_[i] = 0;
    memset(&staged_, 0, sizeof(staged_));
}

CommandResult Controller::fail(long id, const char* op, const char* err) {
    CommandResult r;
    r.id = id;
    r.ok = false;
    r.op = op;
    r.err = err;
    r.field = nullptr;
    r.left = -1;
    r.waitS = 0;
    r.restart = false;
    return r;
}

void Controller::tick(uint32_t nowMs) {
    if (pinSet_ && until(pinExpiresMs_, nowMs) <= 0) pinSet_ = false;
    if (locked_ && until(lockedUntilMs_, nowMs) <= 0) locked_ = false;
    if (authedConn_ >= 0 && until(sessionUntilMs_, nowMs) <= 0) {
        authedConn_ = -1;
        hasStaged_ = false;
    }
}

bool Controller::authed(int connId, uint32_t nowMs) const {
    return authedConn_ >= 0 && authedConn_ == connId && until(sessionUntilMs_, nowMs) > 0;
}

bool Controller::pinActive(uint32_t nowMs) const {
    return pinSet_ && until(pinExpiresMs_, nowMs) > 0;
}

uint32_t Controller::pinRemainingMs(uint32_t nowMs) const {
    return pinActive(nowMs) ? static_cast<uint32_t>(until(pinExpiresMs_, nowMs)) : 0;
}

void Controller::onDisconnect(int connId) {
    if (connId < 0 || authedConn_ == connId) {    // a negative id means every connection
        authedConn_ = -1;
        hasStaged_ = false;
    }
    pinSet_ = false;
    pin_[0] = '\0';
}

CommandResult Controller::handle(const char* json, size_t len, int connId, uint32_t nowMs, uint32_t random,
                                 const EditableSettings& current, Action& action) {
    action.showPin = false;
    action.commit = false;
    field_[0] = '\0';
    tick(nowMs);

    if (!json || len == 0 || len > kMaxCommandLen) return fail(0, nullptr, "bad_json");
    JsonDocument doc;
    if (deserializeJson(doc, json, len) || !doc.is<JsonObject>()) return fail(0, nullptr, "bad_json");
    if (!doc["id"].is<long>() || !doc["op"].is<const char*>()) return fail(0, nullptr, "bad_json");
    const long id = doc["id"].as<long>();
    const char* op = doc["op"].as<const char*>();

    if (!strcmp(op, "pin")) {
        if (locked_) {
            CommandResult r = fail(id, "pin", "locked");
            r.waitS = secondsCeil(static_cast<uint32_t>(until(lockedUntilMs_, nowMs)));
            return r;
        }
        // At most one PIN per 10 s and kPinRequestsPerWindow per 5 minutes, so the screen cannot be kept busy.
        uint32_t waitMs = 0;
        if (anyPinRequested_ && static_cast<uint32_t>(nowMs - lastPinRequestMs_) < kPinRequestGapMs) {
            waitMs = kPinRequestGapMs - static_cast<uint32_t>(nowMs - lastPinRequestMs_);
        }
        if (requestCount_ >= kPinRequestsPerWindow) {
            const uint32_t oldestAge = static_cast<uint32_t>(nowMs - requestTimes_[0]);
            if (oldestAge < kPinRequestWindowMs && kPinRequestWindowMs - oldestAge > waitMs) waitMs = kPinRequestWindowMs - oldestAge;
        }
        if (waitMs > 0) {
            CommandResult r = fail(id, "pin", "too_soon");
            r.waitS = secondsCeil(waitMs);
            return r;
        }
        if (requestCount_ == kPinRequestsPerWindow) {    // forget the oldest request
            for (int i = 1; i < kPinRequestsPerWindow; i++) requestTimes_[i - 1] = requestTimes_[i];
            requestCount_--;
        }
        requestTimes_[requestCount_++] = nowMs;
        snprintf(pin_, sizeof(pin_), "%06lu", static_cast<unsigned long>(random % 1000000u));
        pinSet_ = true;
        pinConn_ = connId;
        pinExpiresMs_ = nowMs + kPinValidMs;
        lastPinRequestMs_ = nowMs;
        anyPinRequested_ = true;
        triesLeft_ = kPinTries - wrongStreak_;    // a new PIN does not bring new tries
        action.showPin = true;
        CommandResult r = fail(id, "pin", nullptr);
        r.ok = true;
        return r;
    }

    if (!strcmp(op, "auth")) {
        if (locked_) {
            CommandResult r = fail(id, "auth", "locked");
            r.waitS = secondsCeil(static_cast<uint32_t>(until(lockedUntilMs_, nowMs)));
            return r;
        }
        if (!pinSet_) return fail(id, "auth", "no_pin");
        const char* given = doc["pin"].is<const char*>() ? doc["pin"].as<const char*>() : "";
        // compare without stopping at the first difference
        unsigned diff = strlen(given) == 6 ? 0u : 1u;
        for (size_t i = 0; i < 6; i++) diff |= static_cast<unsigned>(pin_[i] ^ (i < strlen(given) ? given[i] : 0));
        if (connId != pinConn_) diff |= 1u;    // the PIN belongs to the connection that asked for it
        if (diff == 0) {
            pinSet_ = false;
            pin_[0] = '\0';
            if (authedConn_ != connId) hasStaged_ = false;    // never inherit another connection's unsaved changes
            authedConn_ = connId;
            sessionUntilMs_ = nowMs + kSessionMs;
            wrongStreak_ = 0;
            lockLevel_ = 0;
            CommandResult r = fail(id, "auth", nullptr);
            r.ok = true;
            return r;
        }
        wrongStreak_++;
        triesLeft_ = kPinTries - wrongStreak_;
        if (wrongStreak_ >= kPinTries) {
            // three wrong tries over any number of PINs: lock, and every further lockout lasts twice as long
            uint32_t duration = kLockoutMs;
            for (int i = 0; i < lockLevel_ && duration < kMaxLockoutMs; i++) duration *= 2;
            if (duration > kMaxLockoutMs) duration = kMaxLockoutMs;
            if (lockLevel_ < 16) lockLevel_++;
            wrongStreak_ = 0;
            locked_ = true;
            lockedUntilMs_ = nowMs + duration;
            pinSet_ = false;
            pin_[0] = '\0';
            CommandResult r = fail(id, "auth", "locked");
            r.waitS = secondsCeil(duration);
            return r;
        }
        CommandResult r = fail(id, "auth", "bad_pin");
        r.left = triesLeft_;
        return r;
    }

    const bool known = !strcmp(op, "set") || !strcmp(op, "commit") || !strcmp(op, "discard");
    if (!known) return fail(id, nullptr, "unknown_op");
    // [op] points into the parsed document, which is gone when this function returns: the answer gets its own copy
    copyText(op_, sizeof(op_), op);
    if (!authed(connId, nowMs)) return fail(id, op_, "not_authed");
    sessionUntilMs_ = nowMs + kSessionMs;    // activity keeps the permission alive

    if (!strcmp(op, "discard")) {
        hasStaged_ = false;
        CommandResult r = fail(id, "discard", nullptr);
        r.ok = true;
        return r;
    }

    if (!strcmp(op, "commit")) {
        if (!hasStaged_) return fail(id, "commit", "nothing_staged");
        action.commit = true;
        CommandResult r = fail(id, "commit", nullptr);
        r.ok = true;
        r.restart = true;
        return r;
    }

    // set
    EditableSettings next = hasStaged_ ? staged_ : current;
    bool any = false;
    if (!applyAll(doc.as<JsonObjectConst>(), next, field_, any) || !any) {
        CommandResult r = fail(id, "set", "invalid");
        r.field = field_;
        return r;
    }
    staged_ = next;
    hasStaged_ = true;
    CommandResult r = fail(id, "set", nullptr);
    r.ok = true;
    return r;
}

}  // namespace TB
