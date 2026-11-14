// Host-side test for src/tb_tnc.cpp (what the tracker may transmit on its own in each TNC mode).
// Built and run by tools/tb_test/run.py. Prints "FAIL <message>" lines and a "SUMMARY" line.

#include <cstdio>

#include "../../include/tb_tnc.h"

static int gFailures = 0;
static int gChecks = 0;

static void expectPolicy(const char* name, TB::TncMode mode, bool connected, uint32_t since, bool beacons, bool messaging, bool digipeat,
                         bool manualTx) {
    const TB::TxPolicy p = TB::txPolicyFor(mode, connected, since);
    gChecks++;
    if (p.beacons != beacons || p.messaging != messaging || p.digipeat != digipeat || p.manualTx != manualTx) {
        gFailures++;
        std::printf("FAIL\t%s: got beacons=%d messaging=%d digipeat=%d manual=%d\n", name, p.beacons, p.messaging, p.digipeat, p.manualTx);
    }
}

static void expectMode(const char* name, TB::TncMode mode, bool connected, uint32_t since, TB::TncMode expected) {
    gChecks++;
    if (TB::effectiveTncMode(mode, connected, since) != expected) {
        gFailures++;
        std::printf("FAIL\t%s: effective mode %d, expected %d\n", name, (int)TB::effectiveTncMode(mode, connected, since), (int)expected);
    }
}

int main() {
    const uint32_t longAgo = 10u * 60u * 1000u;
    const uint32_t d = TB::kTncResumeDelayMs;

    // Normal: stock behaviour, with or without a phone
    expectPolicy("normal, phone", TB::kTncNormal, true, 0, true, true, true, true);
    expectPolicy("normal, no phone", TB::kTncNormal, false, longAgo, true, true, true, true);

    // Full: the tracker originates nothing, phone or not
    expectPolicy("full, phone", TB::kTncFull, true, 0, false, false, false, false);
    expectPolicy("full, no phone", TB::kTncFull, false, longAgo, false, false, false, false);
    expectPolicy("full, just disconnected", TB::kTncFull, false, 0, false, false, false, false);

    // Semi: beacons always; messages are the phone's job only while it is connected
    expectPolicy("semi, phone", TB::kTncSemi, true, 0, true, false, true, true);
    expectPolicy("semi, no phone", TB::kTncSemi, false, longAgo, true, true, true, true);
    expectPolicy("semi, just disconnected", TB::kTncSemi, false, 0, true, true, true, true);

    // Smart: Full with a phone, Semi without, switching back after the delay
    expectPolicy("smart, phone", TB::kTncSmart, true, 0, false, false, false, false);
    expectPolicy("smart, phone (since ignored)", TB::kTncSmart, true, longAgo, false, false, false, false);
    // just after the phone has gone: messages are the tracker's again at once, beacons wait for the delay
    expectPolicy("smart, just disconnected", TB::kTncSmart, false, 0, false, true, true, true);
    expectPolicy("smart, 1 ms before the delay", TB::kTncSmart, false, d - 1, false, true, true, true);
    expectPolicy("smart, at the delay", TB::kTncSmart, false, d, true, true, true, true);
    expectPolicy("smart, long after", TB::kTncSmart, false, longAgo, true, true, true, true);
    expectPolicy("smart, never connected since start (huge)", TB::kTncSmart, false, 0xFFFFFFFFu, true, true, true, true);

    expectMode("smart+phone -> full", TB::kTncSmart, true, 0, TB::kTncFull);
    expectMode("smart, waiting -> semi", TB::kTncSmart, false, d - 1, TB::kTncSemi);
    expectMode("smart, alone -> semi", TB::kTncSmart, false, d, TB::kTncSemi);
    expectMode("normal stays", TB::kTncNormal, true, 0, TB::kTncNormal);
    expectMode("full stays", TB::kTncFull, false, longAgo, TB::kTncFull);
    expectMode("semi stays", TB::kTncSemi, true, 0, TB::kTncSemi);

    // numbers from the settings file
    gChecks += 6;
    if (TB::tncModeFrom(0) != TB::kTncNormal || TB::tncModeFrom(1) != TB::kTncFull || TB::tncModeFrom(2) != TB::kTncSemi ||
        TB::tncModeFrom(3) != TB::kTncSmart) {
        gFailures++;
        std::printf("FAIL\ttncModeFrom 0..3\n");
    }
    if (TB::tncModeFrom(-1) != TB::kTncNormal || TB::tncModeFrom(4) != TB::kTncNormal || TB::tncModeFrom(1000) != TB::kTncNormal) {
        gFailures++;
        std::printf("FAIL\tunknown numbers must mean Normal\n");
    }

    std::printf("SUMMARY\t%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
