"""Builds and runs tools/tb_test/test_tb_info.cpp with a host C++ compiler, then checks every payload.

Usage:  python tools/tb_test/run.py            (needs `python-zig` or g++/clang++ on PATH)

Checks, for every payload the C++ side produced:
  - it parses as JSON (strict, UTF-8)
  - its length equals the returned length and is below the capacity
  - it never contains the words password / email / winlink (settings that must stay private)
  - cases with a known expectation have the expected fields
Also scans the firmware glue sources for private setting names.
"""
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SECRETS = re.compile(r"passw|e-?mail|winlink|wlnk", re.I)


def compiler():
    for name in ("g++", "clang++"):
        if shutil.which(name):
            return [name]
    if shutil.which("python-zig"):
        return ["python-zig", "c++"]
    sys.exit("no host C++ compiler found (install ziglang: uv tool install ziglang)")


def build(test_source, sources, out_name, extra_includes=()):
    out = HERE / out_name
    cmd = compiler() + ["-std=c++11", "-I" + str(ROOT / "include")] + ["-I" + str(i) for i in extra_includes] + [
        "-Wall", "-Wextra", "-O1", "-fsanitize=undefined", str(HERE / test_source)] + [str(ROOT / "src" / x) for x in sources] + ["-o", str(out)]
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError:
        # sanitizer runtime is not available everywhere
        subprocess.run([c for c in cmd if not c.startswith("-fsanitize")], check=True)
    return out


def main():
    json_lib = ROOT / ".pio" / "libdeps" / "heltec_wireless_tracker" / "ArduinoJson" / "src"
    if not json_lib.exists():
        sys.exit("ArduinoJson not found; run `pio run -e heltec_wireless_tracker` once to download the libraries")
    info = build("test_tb_info.cpp", ["tb_info.cpp"], "tb_test.exe")
    control = build("test_tb_control.cpp", ["tb_control.cpp", "tb_info.cpp"], "tb_control_test.exe", [json_lib])
    raw = subprocess.run([str(info)], capture_output=True, check=True).stdout
    control_run = subprocess.run([str(control)], capture_output=True)
    raw += control_run.stdout
    if control_run.returncode not in (0, 1) or b"SUMMARY" not in control_run.stdout:
        sys.exit("control test crashed (exit code %s): %s" % (control_run.returncode, control_run.stderr.decode("utf-8", "replace")[-400:]))
    cases = 0
    failures = []
    names = {}
    for line in raw.replace(b"\r\n", b"\n").split(b"\n"):  # Windows text-mode stdout adds \r
        if not line:
            continue
        parts = line.split(b"\t", 4)
        if parts[0] == b"FAIL":
            failures.append("control test: " + line.decode("utf-8", "replace")[5:])
            continue
        if parts[0] == b"SUMMARY":
            print("control:", line.decode("utf-8", "replace").split(chr(9), 1)[1])
            continue
        if parts[0] == b"OVERRUN":
            failures.append("buffer overrun: " + line.decode("utf-8", "replace"))
            continue
        if parts[0] == b"SMALL":
            n, terminated = int(parts[2]), int(parts[3])
            if n != 0 or not terminated:
                failures.append(f"small buffer: returned {n}, terminated={terminated}")
            continue
        _, name, cap, n, payload = parts
        name, cap, n = name.decode(), int(cap), int(n)
        cases += 1
        label = f"{name}(cap={cap})"
        try:
            text = payload.decode("utf-8")
            doc = json.loads(text)
        except Exception as e:  # noqa: BLE001
            if n == 0 and name.startswith("fuzz"):
                continue  # buffer too small to hold even the minimal form: reported as 0 bytes, which is allowed
            failures.append(f"{label}: invalid JSON/UTF-8 ({e}): {payload[:120]!r}")
            continue
        if n == 0:
            failures.append(f"{label}: valid JSON but length 0")
            continue
        if len(payload) != n:
            failures.append(f"{label}: returned length {n} but payload is {len(payload)} bytes")
        if len(payload) >= cap:
            failures.append(f"{label}: payload {len(payload)} bytes does not fit capacity {cap}")
        if SECRETS.search(text):
            failures.append(f"{label}: contains a private setting name")
        if doc.get("v") != 1:
            failures.append(f"{label}: missing protocol version")
        names[name] = doc

    def expect(cond, msg):
        if not cond:
            failures.append(msg)

    info = names.get("info_normal", {})
    expect(info.get("bat", {}).get("p") == 87 and info.get("gps", {}).get("sat") == 9, "info_normal: wrong values")
    expect(info.get("up") == 4000000000, "info_normal: uptime not preserved as unsigned")
    nb = names.get("info_no_battery_no_fix", {})
    expect(nb.get("bat") is None or nb["bat"].get("p") is None, "info_no_battery: battery should be absent/unknown")
    expect(nb.get("gps", {}).get("fix") == 0, "info_no_battery: fix should be 0")
    expect(len(names.get("config_normal", {}).get("b", [])) == 3, "config_normal: expected 3 beacons")
    expect(len(names.get("config_overflow_degrades", {}).get("b", [])) >= 1, "config_overflow: must keep at least 1 beacon")
    expect(names.get("config_empty", {}).get("b") == [], "config_empty: expected empty beacon list")
    first = (names.get("config_overflow_degrades", {}).get("b") or [{}])[0]
    expect(first.get("cc") == 1, "config_overflow: a cut comment must be flagged with cc")
    expect(all("cc" not in x for x in names.get("config_normal", {}).get("b", [])), "config_normal: short comments must not be flagged")
    msgs = names.get("messages_normal", {})
    expect(msgs.get("n") == 6 and 1 <= len(msgs.get("m", [])) <= 6, "messages_normal: wrong counts")
    expect(msgs.get("m", [{}])[0].get("f") == "SP9AAA-7", "messages_normal: newest must be first")
    expect(names.get("messages_none", {}).get("m") == [], "messages_none: expected empty list")
    trunc = names.get("messages_big_truncated", {}).get("m", [])
    expect(len(trunc) == 2 and len(trunc[0]["t"]) == 100, "messages_big_truncated: long text must be cut at 100 characters")
    skipped = names.get("messages_big_skipped", {})
    expect([m["f"] for m in skipped.get("m", [])] == ["OK"], "messages_big_skipped: oversized message must be skipped")

    # The glue must never touch private settings.
    for glue in ("src/tb_ble_info.cpp", "include/tb_ble_info.h", "src/tb_info.cpp", "include/tb_info.h", "src/tb_control.cpp", "include/tb_control.h", "src/tb_ble_control.cpp", "include/tb_ble_control.h"):
        p = ROOT / glue
        if p.exists():
            code = re.sub(r"//.*|/\*.*?\*/", "", p.read_text(encoding="utf-8"), flags=re.S)
            if SECRETS.search(code):
                failures.append(f"{glue}: mentions a private setting (password/email/winlink)")

    print(f"{cases} payloads checked")
    if failures:
        print(f"FAILED ({len(failures)}):")
        for f in failures[:30]:
            print("  -", f)
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
