// Host-side test for src/tb_radio.cpp (the name the tracker shows for a radio setting). Run by tools/tb_test/run.py.

#include <cstdio>
#include <cstring>

#include "../../include/tb_radio.h"

static int gFailures = 0;
static int gChecks = 0;

static void expectLabel(const char* name, long f, long bw, int sf, int cr, const char* shared, const char* expected, size_t maxChars = 11) {
    char buf[16];
    const size_t n = TB::radioLabel(buf, sizeof(buf), f, bw, sf, cr, shared, maxChars);
    gChecks++;
    if (std::strcmp(buf, expected) != 0 || n != std::strlen(expected)) {
        gFailures++;
        std::printf("FAIL\t%s: got \"%s\" (%zu), expected \"%s\"\n", name, buf, n, expected);
    }
}

int main() {
    // known settings follow the values, not a position
    expectLabel("EU", 433775000, 125000, 12, 5, "EU", "EU");
    expectLabel("TR build names the shared setting TR", 433775000, 125000, 12, 5, "TR", "TR");
    expectLabel("PL", 434855000, 125000, 9, 7, "EU", "PL");
    expectLabel("PL in the TR build", 434855000, 125000, 9, 7, "TR", "PL");
    expectLabel("UK", 439912500, 125000, 12, 5, "EU", "UK");
    expectLabel("US", 915000000, 125000, 12, 5, "EU", "US");

    // one number different: not that setting any more
    expectLabel("PL frequency with EU SF/CR is neither", 434855000, 125000, 12, 5, "EU", "434.855");
    expectLabel("EU frequency with PL SF/CR", 433775000, 125000, 9, 7, "EU", "433.775");
    expectLabel("EU values on another bandwidth", 433775000, 250000, 12, 5, "EU", "433.775");
    expectLabel("a custom frequency is shown as MHz", 433900000, 125000, 12, 5, "EU", "433.9");
    expectLabel("UK frequency with another SF", 439912500, 125000, 9, 5, "EU", "439.9125");
    expectLabel("whole MHz keeps one decimal", 868000000, 125000, 12, 5, "EU", "868.0");
    expectLabel("six decimals at most", 433775123, 125000, 12, 5, "EU", "433.775123");

    // short spaces: decimals are dropped, never the integer part
    expectLabel("short, 5 chars", 439912500, 125000, 9, 5, "EU", "439.9", 5);
    expectLabel("short, 3 chars", 439912500, 125000, 9, 5, "EU", "439", 3);
    expectLabel("short, 1 char still whole", 439912500, 125000, 9, 5, "EU", "439", 1);
    expectLabel("a known name ignores the limit", 434855000, 125000, 9, 7, "EU", "PL", 1);

    // robustness
    char tiny[4];
    gChecks++;
    if (TB::radioLabel(tiny, sizeof(tiny), 433775000, 125000, 12, 5, "EU") != 0) {
        gFailures++;
        std::printf("FAIL\ta too small buffer must return 0\n");
    }
    gChecks++;
    if (TB::radioLabel(nullptr, 16, 433775000, 125000, 12, 5, "EU") != 0) {
        gFailures++;
        std::printf("FAIL\ta null buffer must return 0\n");
    }
    char buf[16];
    TB::radioLabel(buf, sizeof(buf), 433775000, 125000, 12, 5, nullptr);
    gChecks++;
    if (std::strcmp(buf, "EU") != 0) {
        gFailures++;
        std::printf("FAIL\tno shared name falls back to EU\n");
    }
    TB::radioLabel(buf, sizeof(buf), -5, 125000, 12, 5, "EU");
    gChecks++;
    if (std::strcmp(buf, "0.0") != 0) {
        gFailures++;
        std::printf("FAIL\tnegative frequency must not crash: got %s\n", buf);
    }
    gChecks++;
    if (TB::radioRegion(433775000, 125000, 12, 5) != TB::kRegionEuTr || TB::radioRegion(1, 1, 1, 1) != TB::kRegionUnknown) {
        gFailures++;
        std::printf("FAIL\tradioRegion\n");
    }

    std::printf("SUMMARY\t%d checks, %d failed\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
