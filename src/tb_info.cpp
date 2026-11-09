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

#include "tb_info.h"

#include <math.h>
#include <stdio.h>

namespace TB {
namespace {

// Writes JSON into a fixed buffer. When something does not fit, ok() turns false and nothing more is
// written; the buffer is always NUL-terminated.
class Writer {
public:
    struct State {
        size_t len;
        bool   ok;
        bool   pendingValue;
        int    depth;
        bool   first[8];
    };

    Writer(char* buf, size_t cap) : buf_(buf), cap_(cap), len_(0), ok_(cap > 0), pendingValue_(false), depth_(0) {
        for (int i = 0; i < 8; i++) first_[i] = true;
        if (cap_ > 0) buf_[0] = '\0';
    }

    bool   ok() const { return ok_; }
    size_t size() const { return len_; }

    State save() const {
        State s;
        s.len = len_;
        s.ok = ok_;
        s.pendingValue = pendingValue_;
        s.depth = depth_;
        for (int i = 0; i < 8; i++) s.first[i] = first_[i];
        return s;
    }

    void restore(const State& s) {
        len_ = s.len;
        ok_ = s.ok;
        pendingValue_ = s.pendingValue;
        depth_ = s.depth;
        for (int i = 0; i < 8; i++) first_[i] = s.first[i];
        if (cap_ > 0) buf_[len_] = '\0';
    }

    void beginObject() { open('{'); }
    void endObject() { close('}'); }
    void beginArray() { open('['); }
    void endArray() { close(']'); }

    void key(const char* k) {
        separator();
        quoted(k, 1000);
        put(':');
        pendingValue_ = true;
    }

    void str(const char* s, size_t maxChars = 1000) {
        separator();
        quoted(s, maxChars);
    }

    void integer(long long v) {
        separator();
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%lld", v);
        raw(tmp);
    }

    void unsignedInteger(unsigned long long v) {
        separator();
        char tmp[24];
        snprintf(tmp, sizeof(tmp), "%llu", v);
        raw(tmp);
    }

    // Fixed decimals; NaN and infinity (never valid JSON) become 0.
    void number(double v, int decimals) {
        separator();
        char tmp[40];
        if (!isfinite(v)) v = 0.0;
        snprintf(tmp, sizeof(tmp), "%.*f", decimals, v);
        raw(tmp);
    }

    void boolean(bool v) {
        separator();
        raw(v ? "1" : "0");
    }

private:
    void put(char c) {
        if (ok_ && len_ + 1 < cap_) {
            buf_[len_++] = c;
            buf_[len_] = '\0';
        } else {
            ok_ = false;
        }
    }

    void raw(const char* s) {
        for (; *s; s++) put(*s);
    }

    void separator() {
        if (pendingValue_) {
            pendingValue_ = false;
            return;
        }
        if (depth_ > 0) {
            if (!first_[depth_ - 1]) put(',');
            first_[depth_ - 1] = false;
        }
    }

    void open(char c) {
        separator();
        put(c);
        if (depth_ < 8) {
            first_[depth_] = true;
            depth_++;
        } else {
            ok_ = false;
        }
    }

    void close(char c) {
        if (depth_ > 0) depth_--;
        put(c);
    }

    // Number of bytes of the valid UTF-8 sequence at s, or 0 when it is not valid.
    static int utf8Length(const unsigned char* s) {
        const unsigned char c = s[0];
        if (c < 0x80) return 1;
        if (c >= 0xC2 && c <= 0xDF) return (s[1] & 0xC0) == 0x80 ? 2 : 0;
        if (c >= 0xE0 && c <= 0xEF) {
            if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return 0;
            if (c == 0xE0 && s[1] < 0xA0) return 0;   // overlong
            if (c == 0xED && s[1] >= 0xA0) return 0;  // UTF-16 surrogates
            return 3;
        }
        if (c >= 0xF0 && c <= 0xF4) {
            if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return 0;
            if (c == 0xF0 && s[1] < 0x90) return 0;   // overlong
            if (c == 0xF4 && s[1] >= 0x90) return 0;  // above U+10FFFF
            return 4;
        }
        return 0;
    }

    // JSON string: escapes control characters, keeps valid UTF-8, replaces anything else with '?',
    // and stops after maxChars characters.
    void quoted(const char* s, size_t maxChars) {
        put('"');
        if (s) {
            const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
            size_t chars = 0;
            while (*p && chars < maxChars && ok_) {
                const unsigned char c = *p;
                if (c == '"' || c == '\\') {
                    put('\\');
                    put(static_cast<char>(c));
                    p++;
                } else if (c == '\n') {
                    raw("\\n");
                    p++;
                } else if (c == '\r') {
                    raw("\\r");
                    p++;
                } else if (c == '\t') {
                    raw("\\t");
                    p++;
                } else if (c < 0x20 || c == 0x7F) {
                    char tmp[8];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                    raw(tmp);
                    p++;
                } else {
                    // The terminator makes reading up to three continuation bytes safe: a NUL is not 10xxxxxx.
                    const int n = utf8Length(p);
                    if (n == 0) {
                        put('?');
                        p++;
                    } else {
                        for (int i = 0; i < n; i++) put(static_cast<char>(p[i]));
                        p += n;
                    }
                }
                chars++;
            }
        }
        put('"');
    }

    char*  buf_;
    size_t cap_;
    size_t len_;
    bool   ok_;
    bool   pendingValue_;
    int    depth_;
    bool   first_[8];
};

size_t finish(const Writer& w) {
    return w.ok() ? w.size() : 0;
}

void writeLora(Writer& w, const LoraData& l) {
    w.beginObject();
    w.key("f");  w.integer(l.freqHz);
    w.key("sf"); w.integer(l.sf);
    w.key("bw"); w.integer(l.bwHz);
    w.key("cr"); w.integer(l.cr);
    w.key("pw"); w.integer(l.powerDbm);
    w.endObject();
}

}  // namespace

size_t buildInfo(const InfoData& d, char* out, size_t cap) {
    Writer w(out, cap);
    w.beginObject();
    w.key("v");    w.integer(kProtocolVersion);
    w.key("fw");   w.str(d.firmware, 24);
    w.key("bd");   w.str(d.board, 32);
    w.key("up");   w.unsignedInteger(d.uptimeS);
    if (d.batteryPresent) {
        w.key("bat");
        w.beginObject();
        w.key("v"); w.number(d.batteryV, 2);
        if (d.batteryPct >= 0) { w.key("p"); w.integer(d.batteryPct); }
        if (d.hasBatteryMa) { w.key("i"); w.integer(d.batteryMa); }
        w.endObject();
    }
    w.key("gps");
    w.beginObject();
    w.key("on");  w.boolean(d.gpsEnabled);
    w.key("fix"); w.boolean(d.gpsFix);
    w.key("sat"); w.integer(d.satellites);
    w.key("hdop"); w.number(d.hdop, 1);
    w.key("kmh"); w.number(d.speedKmh, 1);
    w.key("alt"); w.integer(d.altitudeM);
    w.key("age"); w.integer(d.gpsAgeS);
    w.endObject();
    w.key("tx");
    w.beginObject();
    w.key("ago"); w.integer(d.lastTxAgoS);
    w.key("sb");  w.boolean(d.smartBeacon);
    w.endObject();
    w.key("cs");   w.str(d.callsign, 16);
    w.key("bi");   w.integer(d.beaconIndex);
    w.key("lora"); writeLora(w, LoraData{d.loraFreqHz, d.sf, d.bwHz, d.cr, d.powerDbm});
    w.key("msg");  w.integer(d.storedMessages);
    w.key("heap"); w.unsignedInteger(d.freeHeap);
    w.endObject();
    return finish(w);
}

namespace {

struct Detail {
    size_t beacons;
    size_t commentChars;
    size_t labelChars;
    size_t loras;
};

size_t tryConfig(const ConfigData& c, const Detail& detail, char* out, size_t cap) {
    Writer w(out, cap);
    w.beginObject();
    w.key("v"); w.integer(kProtocolVersion);
    w.key("b");
    w.beginArray();
    const size_t nb = c.beacons ? (c.beaconCount < detail.beacons ? c.beaconCount : detail.beacons) : 0;
    for (size_t i = 0; i < nb; i++) {
        const BeaconData& b = c.beacons[i];
        w.beginObject();
        w.key("cs");  w.str(b.callsign, 16);
        w.key("sym"); w.str(b.symbol, 2);
        w.key("ov");  w.str(b.overlay, 2);
        w.key("sb");  w.boolean(b.smartBeacon);
        w.key("sbs"); w.integer(b.smartSetting);
        w.key("eco"); w.boolean(b.gpsEco);
        if (detail.commentChars > 0) { w.key("cm"); w.str(b.comment, detail.commentChars); }
        if (detail.labelChars > 0) { w.key("lbl"); w.str(b.label, detail.labelChars); }
        w.endObject();
    }
    w.endArray();
    w.key("path"); w.str(c.path, 40);
    w.key("lora");
    w.beginArray();
    const size_t nl = c.lora ? (c.loraCount < detail.loras ? c.loraCount : detail.loras) : 0;
    for (size_t i = 0; i < nl; i++) writeLora(w, c.lora[i]);
    w.endArray();
    w.key("disp");
    w.beginObject();
    w.key("eco"); w.boolean(c.dispEco);
    w.key("to");  w.integer(c.dispTimeoutS);
    w.key("t180"); w.boolean(c.dispTurn180);
    w.endObject();
    w.key("bat");
    w.beginObject();
    w.key("sv");  w.boolean(c.batSendVoltage);
    w.key("mon"); w.boolean(c.batMonitor);
    w.key("sleep"); w.number(c.batSleepV, 2);
    w.endObject();
    w.key("o");
    w.beginObject();
    w.key("simple"); w.boolean(c.simpleMode);
    w.key("cmt");    w.integer(c.commentEvery);
    w.key("rate");   w.integer(c.nonSmartRate);
    w.key("stand");  w.integer(c.standingUpdate);
    w.key("alt");    w.boolean(c.sendAltitude);
    w.key("nogps");  w.boolean(c.disableGps);
    w.key("cad");    w.boolean(c.cad);
    w.endObject();
    w.key("bt");
    w.beginObject();
    w.key("name"); w.str(c.btName, 24);
    w.key("ble");  w.boolean(c.btBle);
    w.key("kiss"); w.boolean(c.btKiss);
    w.endObject();
    w.key("tel"); w.boolean(c.telemetry);
    w.endObject();
    return finish(w);
}

}  // namespace

size_t buildConfig(const ConfigData& c, char* out, size_t cap) {
    // Full detail first, then less and less until it fits.
    const Detail tiers[] = {
        {kMaxBeacons, 24, 12, kMaxLoraTypes},
        {kMaxBeacons, 8, 0, kMaxLoraTypes},
        {kMaxBeacons, 0, 0, kMaxLoraTypes},
        {3, 0, 0, 2},
        {1, 0, 0, 1},
        {0, 0, 0, 0},
    };
    for (size_t i = 0; i < sizeof(tiers) / sizeof(tiers[0]); i++) {
        const size_t n = tryConfig(c, tiers[i], out, cap);
        if (n > 0) return n;
    }
    if (cap > 0) out[0] = '\0';
    return 0;
}

size_t buildMessages(const MessageData* newest, size_t count, int total, char* out, size_t cap) {
    Writer w(out, cap);
    w.beginObject();
    w.key("v"); w.integer(kProtocolVersion);
    w.key("n"); w.integer(total);
    w.key("m");
    w.beginArray();
    if (newest) {
        for (size_t i = 0; i < count; i++) {
            const Writer::State before = w.save();
            w.beginObject();
            w.key("f"); w.str(newest[i].from, 12);
            w.key("t"); w.str(newest[i].text, 100);
            w.endObject();
            // Keep room for the closing "]}" and the terminator; a message that does not fit is dropped whole.
            if (!w.ok() || w.size() + 3 > cap) w.restore(before);
        }
    }
    w.endArray();
    w.endObject();
    return finish(w);
}

}  // namespace TB
