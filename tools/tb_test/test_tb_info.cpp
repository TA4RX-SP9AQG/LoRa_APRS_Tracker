// Host-side test for src/tb_info.cpp (the JSON the tracker offers to the TrackBridge app).
// Built and run by tools/tb_test/run.py, which validates every printed payload with a real JSON parser.
//
// Output format, one line per case:  CASE <name> <capacity> <returned-length> <payload>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../../include/tb_info.h"

// Buffer with guard bytes after the capacity: a builder that writes past [cap] changes them.
static unsigned char gGuard[2048];
static char* guarded() {
    std::memset(gGuard, 0xA5, sizeof(gGuard));
    return reinterpret_cast<char*>(gGuard);
}
static void checkGuard(const char* name, size_t cap) {
    for (size_t i = cap; i < cap + 128 && i < sizeof(gGuard); i++) {
        if (gGuard[i] != 0xA5) {
            std::printf("OVERRUN\t%s\t%zu\t%zu\n", name, cap, i);
            return;
        }
    }
}

static void emit(const char* name, size_t cap, size_t n, const char* payload) {
    std::printf("CASE\t%s\t%zu\t%zu\t%s\n", name, cap, n, payload);
}

static TB::InfoData sampleInfo() {
    TB::InfoData d = {};
    d.firmware = "4.0.1-tb1";
    d.board = "heltec_wireless_tracker";
    d.callsign = "SP9AQG-9";
    d.uptimeS = 4000000000u;
    d.batteryPresent = true;
    d.batteryV = 4.0449f;
    d.batteryPct = 87;
    d.hasBatteryMa = true;
    d.batteryMa = -120;
    d.gpsEnabled = true;
    d.gpsFix = true;
    d.satellites = 9;
    d.hdop = 1.234f;
    d.speedKmh = 3.46f;
    d.altitudeM = 210;
    d.gpsAgeS = 2;
    d.lastTxAgoS = 55;
    d.smartBeacon = true;
    d.beaconIndex = 1;
    d.loraFreqHz = 433775000;
    d.sf = 12;
    d.bwHz = 125000;
    d.cr = 5;
    d.powerDbm = 20;
    d.storedMessages = 3;
    d.freeHeap = 123456;
    return d;
}

static std::vector<TB::BeaconData> sampleBeacons(size_t n, const char* comment) {
    std::vector<TB::BeaconData> v;
    for (size_t i = 0; i < n; i++) {
        TB::BeaconData b = {};
        b.callsign = "SP9AQG-9";
        b.symbol = "[";
        b.overlay = "/";
        b.comment = comment;
        b.label = "walk";
        b.smartBeacon = true;
        b.smartSetting = (int)i;
        v.push_back(b);
    }
    return v;
}

int main() {
    char buf[1024];

    // 1. info: normal
    {
        TB::InfoData d = sampleInfo();
        size_t n = TB::buildInfo(d, buf, TB::kMaxPayload);
        emit("info_normal", TB::kMaxPayload, n, buf);
    }
    // 2. info: no battery, no GPS fix, never transmitted, unknown ages
    {
        TB::InfoData d = sampleInfo();
        d.batteryPresent = false;
        d.hasBatteryMa = false;
        d.batteryPct = -1;
        d.gpsFix = false;
        d.gpsAgeS = -1;
        d.lastTxAgoS = -1;
        size_t n = TB::buildInfo(d, buf, TB::kMaxPayload);
        emit("info_no_battery_no_fix", TB::kMaxPayload, n, buf);
    }
    // 3. info: hostile strings (quotes, backslash, control chars, invalid UTF-8, huge)
    {
        TB::InfoData d = sampleInfo();
        std::string evil = "A\"B\\C\n\r\t\x01\x1f \xff\xfe \xc3\x28 \xe2\x82 ";
        evil += std::string(300, 'x');
        d.callsign = evil.c_str();
        d.firmware = nullptr;
        size_t n = TB::buildInfo(d, buf, TB::kMaxPayload);
        emit("info_hostile_strings", TB::kMaxPayload, n, buf);
    }
    // 4. info: NaN / infinity must never leak into JSON
    {
        TB::InfoData d = sampleInfo();
        d.batteryV = 0.0f / 0.0f;
        d.hdop = 1.0f / 0.0f;
        d.speedKmh = -1.0f / 0.0f;
        size_t n = TB::buildInfo(d, buf, TB::kMaxPayload);
        emit("info_nan_inf", TB::kMaxPayload, n, buf);
    }
    // 5. info: too small a buffer -> returns 0 and still terminates
    {
        TB::InfoData d = sampleInfo();
        char small[40];
        std::memset(small, 'Z', sizeof(small));
        size_t n = TB::buildInfo(d, small, sizeof(small));
        std::printf("SMALL\tinfo\t%zu\t%d\n", n, (int)(std::memchr(small, 0, sizeof(small)) != nullptr));
    }
    // 6. config: 3 beacons, short comments (full detail fits)
    {
        std::vector<TB::BeaconData> b = sampleBeacons(3, "hi");
        TB::LoraData l = {433775000, 12, 125000, 5, 20};
        TB::ConfigData c = {};
        c.beacons = b.data();
        c.beaconCount = b.size();
        c.path = "WIDE1-1";
        c.lora = &l;
        c.loraCount = 1;
        c.btName = "LoRaTracker";
        c.btBle = true;
        c.btKiss = true;
        c.batSleepV = 2.9f;
        c.commentEvery = 10;
        c.nonSmartRate = 15;
        c.standingUpdate = 15;
        c.sendAltitude = true;
        c.cad = true;
        c.dispTimeoutS = 4;
        size_t n = TB::buildConfig(c, buf, TB::kMaxPayload);
        emit("config_normal", TB::kMaxPayload, n, buf);
    }
    // 7. config: many beacons with long comments must degrade, never overflow
    {
        std::vector<TB::BeaconData> b = sampleBeacons(40, "a very long comment that certainly does not fit in the payload at all, sorry");
        std::vector<TB::LoraData> l(20, TB::LoraData{433775000, 12, 125000, 5, 20});
        TB::ConfigData c = {};
        c.beacons = b.data();
        c.beaconCount = b.size();
        c.path = "WIDE1-1,WIDE2-1,WIDE3-3,WIDE4-4,WIDE5-5,WIDE6-6,WIDE7-7";
        c.lora = l.data();
        c.loraCount = l.size();
        c.btName = "LoRaTracker";
        size_t n = TB::buildConfig(c, buf, TB::kMaxPayload);
        emit("config_overflow_degrades", TB::kMaxPayload, n, buf);
    }
    // 8. config: zero beacons / null pointers
    {
        TB::ConfigData c = {};
        size_t n = TB::buildConfig(c, buf, TB::kMaxPayload);
        emit("config_empty", TB::kMaxPayload, n, buf);
    }
    // 9. messages: several, newest first
    {
        std::vector<TB::MessageData> m = {
            {"SP9AAA-7", "newest message"}, {"TA4RX-7", "Merhaba \xc3\xa7ok"}, {"N0CALL", "third"}, {"X", "fourth"},
            {"Y", "fifth"}, {"Z", "sixth message that will probably not fit anymore because the budget is small"},
        };
        size_t n = TB::buildMessages(m.data(), m.size(), 6, buf, TB::kMaxPayload);
        emit("messages_normal", TB::kMaxPayload, n, buf);
    }
    // 10. messages: none; and hostile text
    {
        size_t n = TB::buildMessages(nullptr, 0, 0, buf, TB::kMaxPayload);
        emit("messages_none", TB::kMaxPayload, n, buf);
        std::vector<TB::MessageData> m = {{"A\"B", "q\"uote \\ back \n new \xff bad"}};
        n = TB::buildMessages(m.data(), m.size(), 1, buf, TB::kMaxPayload);
        emit("messages_hostile", TB::kMaxPayload, n, buf);
    }
    // 11. messages: one message larger than the whole budget is skipped (not cut mid-way)
    {
        std::string big(2000, 'q');
        std::vector<TB::MessageData> m = {{"BIG", big.c_str()}, {"OK", "small"}};
        // text is cut at 100 characters; with a small buffer the long first message no longer fits and is dropped whole
        size_t n = TB::buildMessages(m.data(), m.size(), 2, buf, TB::kMaxPayload);
        emit("messages_big_truncated", TB::kMaxPayload, n, buf);
        n = TB::buildMessages(m.data(), m.size(), 2, buf, 120);
        emit("messages_big_skipped", 120, n, buf);
    }
    // 12. random fuzz: random bytes in every string, random capacities
    {
        std::srand(12345);
        for (int round = 0; round < 400; round++) {
            std::string a, b, c;
            auto rnd = [](std::string& s, int len) {
                s.clear();
                for (int i = 0; i < len; i++) s.push_back((char)(1 + std::rand() % 255));
            };
            rnd(a, std::rand() % 120);
            rnd(b, std::rand() % 120);
            rnd(c, std::rand() % 400);
            size_t cap = 60 + (size_t)(std::rand() % 460);

            TB::InfoData d = sampleInfo();
            d.callsign = a.c_str();
            d.board = b.c_str();
            char* g = guarded();
            size_t n = TB::buildInfo(d, g, cap);
            checkGuard("fuzz_info", cap);
            emit("fuzz_info", cap, n, g);

            TB::MessageData m[3] = {{a.c_str(), c.c_str()}, {b.c_str(), a.c_str()}, {c.c_str(), b.c_str()}};
            g = guarded();
            n = TB::buildMessages(m, 3, 3, g, cap);
            checkGuard("fuzz_msgs", cap);
            emit("fuzz_msgs", cap, n, g);

            std::vector<TB::BeaconData> bs = sampleBeacons(1 + std::rand() % 6, c.c_str());
            TB::LoraData l = {433775000, 12, 125000, 5, 20};
            TB::ConfigData cfg = {};
            cfg.beacons = bs.data();
            cfg.beaconCount = bs.size();
            cfg.path = a.c_str();
            cfg.lora = &l;
            cfg.loraCount = 1;
            cfg.btName = b.c_str();
            g = guarded();
            n = TB::buildConfig(cfg, g, cap);
            checkGuard("fuzz_cfg", cap);
            emit("fuzz_cfg", cap, n, g);
        }
    }
    return 0;
}
