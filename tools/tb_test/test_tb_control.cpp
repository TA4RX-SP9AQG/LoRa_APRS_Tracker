// Host-side test for src/tb_control.cpp (changing settings from the app). Built and run by tools/tb_test/run.py.
// Prints "FAIL <message>" for every broken expectation and "CASE ..." lines with the result JSON for the runner to parse.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../../include/tb_control.h"

static int gFailures = 0;
static int gChecks = 0;

#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        gChecks++;                                                        \
        if (!(cond)) {                                                    \
            gFailures++;                                                  \
            std::printf("FAIL\t%s (line %d)\n", msg, __LINE__);           \
        }                                                                 \
    } while (0)

static TB::EditableSettings sample() {
    TB::EditableSettings s;
    std::memset(&s, 0, sizeof(s));
    s.beaconCount = 2;
    std::strcpy(s.beacons[0].callsign, "NOCALL-7");
    std::strcpy(s.beacons[0].symbol, "[");
    std::strcpy(s.beacons[0].overlay, "/");
    std::strcpy(s.beacons[1].callsign, "NOCALL-8");
    std::strcpy(s.beacons[1].symbol, ">");
    std::strcpy(s.beacons[1].overlay, "/");
    std::strcpy(s.path, "WIDE1-1");
    s.loraCount = 1;
    s.lora[0] = {433775000, 12, 125000, 5, 20};
    s.dispTimeoutS = 4;
    s.batSleepV = 2.9f;
    s.commentEvery = 10;
    s.nonSmartRate = 15;
    s.standingUpdate = 15;
    std::strcpy(s.btName, "LoRaTracker");
    return s;
}

static void emit(const char* name, const TB::CommandResult& r) {
    char buf[512];
    size_t n = TB::buildCommandResult(r, buf, TB::kMaxPayload);
    std::printf("CASE\t%s\t%d\t%zu\t%s\n", name, (int)TB::kMaxPayload, n, buf);
}

struct Rig {
    TB::Controller c;
    TB::EditableSettings current = sample();
    uint32_t now = 100000;
    long id = 0;
    TB::Action act;

    TB::CommandResult send(const std::string& json, int conn = 1, uint32_t rnd = 123456) {
        act = TB::Action{};
        id++;
        return c.handle(json.c_str(), json.size(), conn, now, rnd, current, act);
    }
    // Request a PIN and sign in; returns the result of "auth".
    TB::CommandResult signIn(int conn = 1) {
        TB::CommandResult r = send("{\"id\":1,\"op\":\"pin\"}", conn, 424242);
        (void)r;
        std::string pin = c.pin();
        return send("{\"id\":2,\"op\":\"auth\",\"pin\":\"" + pin + "\"}", conn);
    }
};

static bool is(const TB::CommandResult& r, bool ok, const char* err) {
    if (r.ok != ok) return false;
    if (!ok && (!r.err || std::strcmp(r.err, err) != 0)) return false;
    return true;
}

int main() {
    // ---- authentication ----
    {
        Rig t;
        CHECK(is(t.send("{\"id\":1,\"op\":\"set\",\"path\":\"WIDE1-1\"}"), false, "not_authed"), "set before sign-in is refused");
        CHECK(is(t.send("{\"id\":1,\"op\":\"commit\"}"), false, "not_authed"), "commit before sign-in is refused");
        CHECK(is(t.send("{\"id\":1,\"op\":\"auth\",\"pin\":\"000000\"}"), false, "no_pin"), "auth without a PIN request is refused");
    }
    {
        Rig t;
        TB::CommandResult r = t.send("{\"id\":7,\"op\":\"pin\"}", 1, 98765);
        CHECK(r.ok && t.act.showPin, "pin request shows the PIN on the screen");
        CHECK(std::strlen(t.c.pin()) == 6, "PIN has six digits");
        CHECK(std::strcmp(t.c.pin(), "098765") == 0, "PIN is zero padded from the random number");
        CHECK(r.id == 7, "result carries the command id");
        emit("result_pin", r);
        CHECK(t.c.pinActive(t.now), "PIN is active");
        CHECK(t.c.pinRemainingMs(t.now) == TB::kPinValidMs, "PIN lasts 60 s");
        // too soon again
        TB::CommandResult again = t.send("{\"id\":8,\"op\":\"pin\"}", 1, 111111);
        CHECK(is(again, false, "too_soon") && again.waitS > 0, "a second PIN request within 10 s is refused");
        emit("result_too_soon", again);
        t.now += TB::kPinRequestGapMs + 1;
        CHECK(t.send("{\"id\":9,\"op\":\"pin\"}", 1, 111111).ok, "PIN can be requested again after 10 s");
        CHECK(std::strcmp(t.c.pin(), "111111") == 0, "the new PIN replaces the old one");
    }
    {
        Rig t;
        TB::CommandResult ok = t.signIn();
        CHECK(ok.ok, "correct PIN signs in");
        emit("result_auth", ok);
        // single use
        CHECK(is(t.send("{\"id\":3,\"op\":\"auth\",\"pin\":\"424242\"}"), false, "no_pin"), "a PIN works only once");
    }
    {
        Rig t;
        t.send("{\"id\":1,\"op\":\"pin\"}", 1, 424242);
        TB::CommandResult r1 = t.send("{\"id\":2,\"op\":\"auth\",\"pin\":\"111111\"}");
        CHECK(is(r1, false, "bad_pin") && r1.left == 2, "first wrong PIN: 2 tries left");
        emit("result_bad_pin", r1);
        TB::CommandResult r2 = t.send("{\"id\":3,\"op\":\"auth\",\"pin\":\"222222\"}");
        CHECK(is(r2, false, "bad_pin") && r2.left == 1, "second wrong PIN: 1 try left");
        TB::CommandResult r3 = t.send("{\"id\":4,\"op\":\"auth\",\"pin\":\"333333\"}");
        CHECK(is(r3, false, "locked") && r3.waitS == 60, "third wrong PIN locks for 60 s");
        emit("result_locked", r3);
        // even the right PIN no longer works, and a new PIN cannot be requested during the lockout
        CHECK(is(t.send("{\"id\":5,\"op\":\"auth\",\"pin\":\"424242\"}"), false, "locked"), "right PIN is refused while locked");
        CHECK(is(t.send("{\"id\":6,\"op\":\"pin\"}", 1, 5), false, "locked"), "no new PIN during the lockout");
        t.now += TB::kLockoutMs + 1;
        CHECK(is(t.send("{\"id\":7,\"op\":\"auth\",\"pin\":\"424242\"}"), false, "no_pin"), "the old PIN is dead after a lockout");
        CHECK(t.send("{\"id\":8,\"op\":\"pin\"}", 1, 777777).ok, "after the lockout a new PIN can be requested");
    }
    {
        Rig t;
        t.send("{\"id\":1,\"op\":\"pin\"}", 1, 424242);
        t.now += TB::kPinValidMs + 1;
        CHECK(is(t.send("{\"id\":2,\"op\":\"auth\",\"pin\":\"424242\"}"), false, "no_pin"), "PIN expires after 60 s");
        CHECK(!t.c.pinActive(t.now), "expired PIN is not shown any more");
    }
    {
        Rig t;
        CHECK(t.signIn(1).ok, "sign in on connection 1");
        CHECK(is(t.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE1-1\"}", 2), false, "not_authed"), "another connection is not signed in");
        t.c.onDisconnect(1);
        CHECK(is(t.send("{\"id\":4,\"op\":\"set\",\"path\":\"WIDE1-1\"}", 1), false, "not_authed"), "disconnect ends the permission");
    }
    {
        Rig t;
        CHECK(t.signIn().ok, "sign in");
        t.now += TB::kSessionMs - 1000;
        CHECK(t.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE2-2\"}").ok, "activity inside the session works");
        t.now += TB::kSessionMs - 1000;
        CHECK(t.send("{\"id\":4,\"op\":\"set\",\"path\":\"WIDE1-1\"}").ok, "activity extends the session");
        t.now += TB::kSessionMs + 1;
        CHECK(is(t.send("{\"id\":5,\"op\":\"set\",\"path\":\"WIDE1-1\"}"), false, "not_authed"), "an idle session ends after 5 minutes");
    }

    // ---- staging and commit ----
    {
        Rig t;
        t.signIn();
        CHECK(is(t.send("{\"id\":3,\"op\":\"commit\"}"), false, "nothing_staged"), "commit without changes is refused");
        TB::CommandResult r = t.send("{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-9\",\"cm\":\"hello\",\"sb\":1,\"sbs\":2}}");
        CHECK(r.ok, "valid beacon change is staged");
        CHECK(std::strcmp(t.c.staged().beacons[0].callsign, "SP9AQG-9") == 0, "callsign staged");
        CHECK(std::strcmp(t.c.staged().beacons[0].comment, "hello") == 0, "comment staged");
        CHECK(t.c.staged().beacons[0].smartBeacon && t.c.staged().beacons[0].smartSetting == 2, "smart beacon staged");
        CHECK(std::strcmp(t.current.beacons[0].callsign, "NOCALL-7") == 0, "the live settings are untouched until commit");
        CHECK(t.send("{\"id\":5,\"op\":\"set\",\"path\":\"WIDE1-1,WIDE2-1\"}").ok, "a second section is staged on top");
        CHECK(std::strcmp(t.c.staged().beacons[0].callsign, "SP9AQG-9") == 0, "earlier staged change is kept");
        TB::CommandResult c = t.send("{\"id\":6,\"op\":\"commit\"}");
        CHECK(c.ok && c.restart && t.act.commit, "commit asks for a save and restart");
        emit("result_commit", c);
    }
    {
        Rig t;
        t.signIn();
        t.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE2-2\"}");
        CHECK(t.send("{\"id\":4,\"op\":\"discard\"}").ok, "discard works");
        CHECK(is(t.send("{\"id\":5,\"op\":\"commit\"}"), false, "nothing_staged"), "nothing left to commit after discard");
    }
    {
        Rig t;
        t.signIn();
        t.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE2-2\"}");
        t.c.onDisconnect(1);
        t.now += TB::kPinRequestGapMs + 1;    // a new PIN is needed, and PIN requests are rate limited
        CHECK(t.signIn().ok, "sign in again after the disconnect");
        CHECK(is(t.send("{\"id\":9,\"op\":\"commit\"}"), false, "nothing_staged"), "unsaved changes are dropped with the connection");
    }

    // ---- validation: atomic and strict ----
    {
        Rig t;
        t.signIn();
        TB::CommandResult r = t.send("{\"id\":3,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-9\",\"sym\":\"toolong\"}}");
        CHECK(is(r, false, "invalid") && r.field && std::strstr(r.field, "sym"), "bad symbol names the field");
        emit("result_invalid", r);
        CHECK(!t.c.hasStaged() || std::strcmp(t.c.staged().beacons[0].callsign, "NOCALL-7") == 0, "nothing of a rejected patch is kept");
        const char* bad[] = {
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"AB\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-16\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"sp9aqg-9\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"TOOLONGCALL-1\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":5,\"cs\":\"SP9AQG-9\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"cs\":\"SP9AQG-9\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cm\":\"pipe | not allowed\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cm\":\"tilde ~ not allowed\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"sbs\":3}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"sb\":\"yes\"}}",
            "{\"id\":4,\"op\":\"set\",\"path\":\"WIDE1-1,WIDE2-1,WIDE3-3,WIDE4-4\"}",
            "{\"id\":4,\"op\":\"set\",\"path\":\"WIDE 1-1\"}",
            "{\"id\":4,\"op\":\"set\",\"path\":\"WIDE1-1*\"}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"f\":100000000}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"f\":1100000000}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"sf\":4}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"sf\":13}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"bw\":123}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"cr\":4}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"pw\":23}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"pw\":-1}}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":1,\"f\":433775000}}",
            "{\"id\":4,\"op\":\"set\",\"disp\":{\"to\":0}}",
            "{\"id\":4,\"op\":\"set\",\"bat\":{\"sleep\":1.0}}",
            "{\"id\":4,\"op\":\"set\",\"o\":{\"rate\":0}}",
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"name\":\"\"}}",
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"name\":\"bad\\\"name\"}}",
            // things that must never be reachable: secrets, switching Bluetooth off, unknown keys
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"ble\":0}}",
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"kiss\":0}}",
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"active\":0}}",
            "{\"id\":4,\"op\":\"set\",\"wifi\":{\"password\":\"x\"}}",
            "{\"id\":4,\"op\":\"set\",\"password\":\"x\"}",
            "{\"id\":4,\"op\":\"set\",\"o\":{\"email\":\"a@b.c\"}}",
            "{\"id\":4,\"op\":\"set\",\"winlink\":{\"password\":\"x\"}}",
            "{\"id\":4,\"op\":\"set\"}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"zzz\":1}}",
        };
        for (const char* json : bad) {
            Rig u;
            u.signIn();
            TB::CommandResult x = u.send(json);
            if (!is(x, false, "invalid")) {
                gFailures++;
                std::printf("FAIL\tshould be rejected: %s\n", json);
            }
            gChecks++;
            if (u.c.hasStaged() && std::memcmp(&u.c.staged(), &u.current, sizeof(u.current)) != 0) {
                gFailures++;
                std::printf("FAIL\trejected patch changed the staged settings: %s\n", json);
            }
        }
        const char* good[] = {
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":1,\"cs\":\"TA4RX-7\",\"sym\":\"b\",\"ov\":\"/\",\"cm\":\"Test 73\",\"lbl\":\"bike\",\"sb\":true,\"sbs\":1,\"eco\":0}}",
            "{\"id\":4,\"op\":\"set\",\"path\":\"\"}",
            "{\"id\":4,\"op\":\"set\",\"path\":\"WIDE1-1,WIDE2-1\"}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"f\":433775000,\"sf\":9,\"bw\":250000,\"cr\":8,\"pw\":22}}",
            "{\"id\":4,\"op\":\"set\",\"disp\":{\"eco\":1,\"to\":30,\"t180\":1}}",
            "{\"id\":4,\"op\":\"set\",\"bat\":{\"sv\":1,\"mon\":1,\"sleep\":3.1}}",
            "{\"id\":4,\"op\":\"set\",\"o\":{\"simple\":0,\"cmt\":5,\"rate\":20,\"stand\":30,\"alt\":1,\"nogps\":0,\"cad\":1}}",
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"name\":\"Tracker-SP9 1\"}}",
            "{\"id\":4,\"op\":\"set\",\"tel\":1}",
        };
        for (const char* json : good) {
            Rig u;
            u.signIn();
            TB::CommandResult x = u.send(json);
            if (!x.ok) {
                gFailures++;
                std::printf("FAIL\tshould be accepted: %s (err=%s field=%s)\n", json, x.err ? x.err : "", x.field ? x.field : "");
            }
            gChecks++;
        }
        Rig u;
        u.signIn();
        u.send("{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"f\":434000000,\"sf\":9,\"bw\":250000,\"cr\":8,\"pw\":22}}");
        CHECK(u.c.staged().lora[0].freqHz == 434000000 && u.c.staged().lora[0].sf == 9 && u.c.staged().lora[0].bwHz == 250000 &&
                  u.c.staged().lora[0].cr == 8 && u.c.staged().lora[0].powerDbm == 22,
              "all radio values staged");
        u.send("{\"id\":5,\"op\":\"set\",\"bat\":{\"sleep\":3.1}}");
        CHECK(u.c.staged().batSleepV > 3.09f && u.c.staged().batSleepV < 3.11f, "float staged");
        u.send("{\"id\":6,\"op\":\"set\",\"bt\":{\"name\":\"Tracker-SP9 1\"}}");
        CHECK(std::strcmp(u.c.staged().btName, "Tracker-SP9 1") == 0, "bluetooth name staged");
    }

    // ---- malformed input ----
    {
        Rig t;
        const char* junk[] = {"", "not json", "[1,2,3]", "{\"id\":1}", "{\"id\":1,\"op\":\"nope\"}", "{\"id\":\"x\",\"op\":\"pin\"}", "{\"op\":\"pin\"}",
                              "{\"id\":1,\"op\":5}"};
        for (const char* j : junk) {
            TB::CommandResult x = t.send(j);
            gChecks++;
            if (x.ok) {
                gFailures++;
                std::printf("FAIL\tjunk accepted: %s\n", j);
            }
        }
        TB::CommandResult x = t.send("not json");
        CHECK(is(x, false, "bad_json"), "garbage is reported as bad_json");
        emit("result_bad_json", x);
        TB::CommandResult y = t.send("{\"id\":1,\"op\":\"nope\"}");
        CHECK(is(y, false, "unknown_op"), "unknown command is reported");
        std::string huge = "{\"id\":1,\"op\":\"set\",\"path\":\"" + std::string(5000, 'A') + "\"}";
        CHECK(!t.send(huge).ok, "oversized command is refused");
    }

    // ---- fuzz: random bytes never crash and never change anything ----
    {
        std::srand(4242);
        Rig t;
        t.signIn();
        TB::EditableSettings before = t.current;
        for (int round = 0; round < 3000; round++) {
            std::string s;
            int len = std::rand() % 200;
            for (int i = 0; i < len; i++) s.push_back((char)(1 + std::rand() % 255));
            if (round % 3 == 0) s = "{\"id\":1,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"" + s + "\"}}";
            if (round % 3 == 1) s = "{\"id\":1,\"op\":\"set\",\"bt\":{\"name\":\"" + s + "\"}}";
            TB::CommandResult x = t.send(s);
            char buf[512];
            size_t n = TB::buildCommandResult(x, buf, TB::kMaxPayload);
            gChecks++;
            if (n == 0) {
                gFailures++;
                std::printf("FAIL\tfuzz: result could not be built\n");
            }
            if (round % 500 == 0) emit("fuzz_result", x);
        }
        CHECK(std::memcmp(&t.current, &before, sizeof(before)) == 0, "fuzz never changes the live settings");
    }

    // ---- review findings ----
    {
        // a refused command must still be answerable: its op name may not point into freed memory
        Rig t;
        TB::CommandResult r = t.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE1-1\"}");
        CHECK(is(r, false, "not_authed") && r.op && std::strcmp(r.op, "set") == 0, "not_authed answer carries the op name");
        emit("result_not_authed", r);
    }
    {
        // wrong tries are counted over all PINs: asking for a new PIN gives no new tries
        Rig t;
        t.send("{\"id\":1,\"op\":\"pin\"}", 1, 111111);
        t.send("{\"id\":2,\"op\":\"auth\",\"pin\":\"000001\"}");
        TB::CommandResult second = t.send("{\"id\":3,\"op\":\"auth\",\"pin\":\"000002\"}");
        CHECK(is(second, false, "bad_pin") && second.left == 1, "two wrong tries used");
        t.now += TB::kPinRequestGapMs + 1;
        CHECK(t.send("{\"id\":4,\"op\":\"pin\"}", 1, 222222).ok, "a new PIN can be requested");
        TB::CommandResult third = t.send("{\"id\":5,\"op\":\"auth\",\"pin\":\"000003\"}");
        CHECK(is(third, false, "locked") && third.waitS == 60, "the third wrong try overall locks, new PIN or not");
    }
    {
        // lockouts get longer until a successful sign-in
        Rig t;
        auto lockOnce = [&t](uint32_t expectedWaitS) {
            t.now += TB::kMaxLockoutMs + 1;    // earlier lockout is over; also clears the request window
            t.c.tick(t.now);
            TB::CommandResult p = t.send("{\"id\":1,\"op\":\"pin\"}", 1, 555555);
            if (!p.ok) {
                gFailures++;
                std::printf("FAIL\tcould not request a PIN for the lockout test (%s)\n", p.err ? p.err : "");
                return;
            }
            TB::CommandResult last{};
            for (int i = 0; i < 3; i++) last = t.send("{\"id\":2,\"op\":\"auth\",\"pin\":\"000000\"}");
            gChecks++;
            if (!(is(last, false, "locked") && (uint32_t)last.waitS == expectedWaitS)) {
                gFailures++;
                std::printf("FAIL\tlockout should last %u s but says %d (%s)\n", expectedWaitS, last.waitS, last.err ? last.err : "");
            }
        };
        lockOnce(60);
        lockOnce(120);
        lockOnce(240);
        for (int i = 0; i < 8; i++) lockOnce(i < 2 ? (60u << (3 + i)) : 1800u);
    }
    {
        // a successful sign-in forgives earlier failures
        Rig t;
        t.send("{\"id\":1,\"op\":\"pin\"}", 1, 424242);
        t.send("{\"id\":2,\"op\":\"auth\",\"pin\":\"000001\"}");
        t.send("{\"id\":3,\"op\":\"auth\",\"pin\":\"424242\"}");
        t.now += TB::kPinRequestGapMs + 1;
        t.send("{\"id\":4,\"op\":\"pin\"}", 1, 313131);
        TB::CommandResult r = t.send("{\"id\":5,\"op\":\"auth\",\"pin\":\"000001\"}");
        CHECK(is(r, false, "bad_pin") && r.left == 2, "after a success the full number of tries is back");
    }
    {
        // the PIN belongs to the connection that asked for it
        Rig t;
        t.send("{\"id\":1,\"op\":\"pin\"}", 1, 424242);
        TB::CommandResult r = t.send("{\"id\":2,\"op\":\"auth\",\"pin\":\"424242\"}", 2);
        CHECK(is(r, false, "bad_pin"), "the right PIN from another connection is refused");
        CHECK(is(t.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE1-1\"}", 2), false, "not_authed"), "and it does not open a session");
    }
    {
        // at most three PIN requests per five minutes
        Rig t;
        for (int i = 0; i < 3; i++) {
            CHECK(t.send("{\"id\":1,\"op\":\"pin\"}", 1, 100000 + i).ok, "first three requests are fine");
            t.now += TB::kPinRequestGapMs + 1;
        }
        TB::CommandResult r = t.send("{\"id\":9,\"op\":\"pin\"}", 1, 999999);
        CHECK(is(r, false, "too_soon") && r.waitS > 200, "the fourth request inside five minutes is refused with a long wait");
        t.now += TB::kPinRequestWindowMs;
        CHECK(t.send("{\"id\":10,\"op\":\"pin\"}", 1, 888888).ok, "later it works again");
    }
    {
        // time arithmetic across the 32 bit millisecond wrap-around
        Rig t;
        t.now = 0xFFFFF000u;
        t.send("{\"id\":1,\"op\":\"pin\"}", 1, 424242);
        CHECK(t.c.pinActive(t.now), "PIN active just before the wrap");
        t.now += 30000u;    // wraps to a small number
        CHECK(t.c.pinActive(t.now) && t.c.pinRemainingMs(t.now) > 29000u && t.c.pinRemainingMs(t.now) < 31000u, "PIN time survives the wrap");
        CHECK(t.send("{\"id\":2,\"op\":\"auth\",\"pin\":\"424242\"}").ok, "sign in after the wrap");
        t.now += TB::kSessionMs - 1000u;
        CHECK(t.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE1-1\"}").ok, "session still alive");
        t.now += TB::kSessionMs + 5u;
        CHECK(is(t.send("{\"id\":4,\"op\":\"set\",\"path\":\"WIDE1-1\"}"), false, "not_authed"), "session ends");
    }
    {
        // session and PIN end by time alone (tick), and onDisconnect(-1) means every connection
        Rig t;
        t.signIn();
        t.now += TB::kSessionMs + 1;
        t.c.tick(t.now);
        CHECK(is(t.send("{\"id\":3,\"op\":\"commit\"}"), false, "not_authed"), "tick ends an idle session");
        Rig u;
        u.signIn(7);
        u.c.onDisconnect(-1);
        CHECK(is(u.send("{\"id\":3,\"op\":\"set\",\"path\":\"WIDE1-1\"}", 7), false, "not_authed"), "onDisconnect(-1) ends every session");
    }
    {
        // values on the edges
        Rig t;
        t.signIn();
        const char* ok[] = {
            "{\"id\":4,\"op\":\"set\",\"bat\":{\"sleep\":2.5}}", "{\"id\":4,\"op\":\"set\",\"bat\":{\"sleep\":4.0}}", "{\"id\":4,\"op\":\"set\",\"bat\":{\"sleep\":4}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cm\":\"123456789012345678901234567890123456789012345678901234567890\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"lbl\":\"12345678901234567890\"}}",
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"name\":\"123456789012345678901234\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"ov\":\"A\"}}", "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"ov\":\"\\\\\"}}",
            "{\"id\":4,\"op\":\"set\",\"path\":\"WIDE2,RS0ISS\"}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-0\"}}", "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-10\"}}",
        };
        for (const char* j : ok) {
            TB::CommandResult x = t.send(j);
            gChecks++;
            if (!x.ok) {
                gFailures++;
                std::printf("FAIL\tshould be accepted: %s (field=%s)\n", j, x.field ? x.field : "");
            }
        }
        const char* bad[] = {
            "{\"id\":4,\"op\":\"set\",\"bat\":{\"sleep\":4.1}}", "{\"id\":4,\"op\":\"set\",\"bat\":{\"sleep\":2.4}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cm\":\"1234567890123456789012345678901234567890123456789012345678901\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"lbl\":\"123456789012345678901\"}}",
            "{\"id\":4,\"op\":\"set\",\"bt\":{\"name\":\"1234567890123456789012345\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"ov\":\"a\"}}", "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"ov\":\"//\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-01\"}}", "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-00\"}}",
            "{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"sb\":2}}", "{\"id\":4,\"op\":\"set\",\"tel\":2}",
            "{\"id\":4,\"op\":\"set\",\"l\":{\"i\":0,\"pw\":22.5}}",
        };
        for (const char* j : bad) {
            Rig u;
            u.signIn();
            TB::CommandResult x = u.send(j);
            gChecks++;
            if (!is(x, false, "invalid")) {
                gFailures++;
                std::printf("FAIL\tshould be rejected: %s\n", j);
            }
        }
        // no beacons / radio profiles known: nothing to address
        Rig z;
        z.current.beaconCount = 0;
        z.current.loraCount = 0;
        z.signIn();
        CHECK(is(z.send("{\"id\":4,\"op\":\"set\",\"b\":{\"i\":0,\"cs\":\"SP9AQG-9\"}}"), false, "invalid"), "no beacon to change");
        CHECK(is(z.send("{\"id\":5,\"op\":\"set\",\"l\":{\"i\":0,\"pw\":10}}"), false, "invalid"), "no radio profile to change");
        // the last of two equal keys wins, and still goes through the same checks
        Rig d;
        d.signIn();
        CHECK(is(d.send("{\"id\":4,\"op\":\"set\",\"path\":\"WIDE1-1\",\"path\":\"bad path\"}"), false, "invalid"), "duplicate keys cannot sneak a bad value in");
    }
    {
        // radio limits of this board's chip (SX1262 in the PC tests)
        CHECK(TB::kFreqMinHz == 150000000L && TB::kFreqMaxHz == 960000000L && TB::kPowerMaxDbm == 22 && TB::kSfMin == 5, "SX1262 limits");
    }

    // ---- individual validators ----
    CHECK(TB::validCallsign("SP9AQG-9") && TB::validCallsign("TA4RX-7") && TB::validCallsign("N0CALL") && TB::validCallsign("SP9AQG-15"),
          "good callsigns");
    CHECK(!TB::validCallsign("") && !TB::validCallsign("AB") && !TB::validCallsign("SP9AQG-16") && !TB::validCallsign("SP9AQG--1") &&
              !TB::validCallsign("SP9 AQG") && !TB::validCallsign("sp9aqg"),
          "bad callsigns");
    CHECK(TB::validPath("") && TB::validPath("WIDE1-1") && TB::validPath("WIDE1-1,WIDE2-1"), "good paths");
    CHECK(!TB::validPath("WIDE1-1,") && !TB::validPath(",WIDE1-1") && !TB::validPath("WIDE-1-1"), "bad paths");
    CHECK(TB::validComment("") && TB::validComment("73 de SP9AQG"), "good comments");
    CHECK(!TB::validComment("caf\xc3\xa9") && !TB::validComment("a|b"), "bad comments");
    CHECK(TB::validBtName("LoRaTracker") && !TB::validBtName("") && !TB::validBtName("a\"b"), "bluetooth names");

    std::printf("SUMMARY\t%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
