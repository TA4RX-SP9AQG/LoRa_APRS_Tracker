"""Packs the PlatformIO build output into the web flasher (docs/) after checking it.

Usage:
    pio run -e heltec_wireless_tracker -e heltec_wireless_tracker_tr
    python tools/build_flasher.py [--ewt path/to/esp-web-tools-package] [--tested]

For each language (en / tr) it
  - refuses to continue unless the images look right (ESP32-S3 chip id, partition table matches what the
    firmware expects, application fits its partition),
  - copies bootloader.bin, partitions.bin, boot_app0.bin, firmware.bin to docs/firmware/<lang>/,
  - writes docs/manifest-<lang>.json for ESP Web Tools and docs/firmware/version.json (with SHA-256 sums).
--tested marks the build as tested on real hardware (the page shows a warning until you pass it).
"""
import datetime
import hashlib
import json
import re
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
ENVS = {"en": "heltec_wireless_tracker", "tr": "heltec_wireless_tracker_tr"}
BOOT_APP0 = Path.home() / ".platformio" / "packages" / "framework-arduinoespressif32" / "tools" / "partitions" / "boot_app0.bin"

# ESP32-S3 flash layout used by this firmware (huge_app.csv)
PARTS = [("bootloader.bin", 0x0), ("partitions.bin", 0x8000), ("boot_app0.bin", 0xE000), ("firmware.bin", 0x10000)]
CHIP_ID_ESP32S3 = 9
APP_OFFSET, APP_SIZE = 0x10000, 0x300000
SPIFFS_OFFSET, SPIFFS_SIZE = 0x310000, 0xE0000   # settings and messages: must stay where the stock firmware keeps them


def write_lf(path, text):
    # always LF, also on Windows (a CRLF checksum list would not verify on Linux)
    Path(path).write_bytes(text.encode("utf-8"))


def fail(message):
    sys.exit("REFUSED: " + message)


def check_image(name, data):
    if data[0] != 0xE9:
        fail(name + " is not an ESP image (bad magic byte)")
    chip = struct.unpack_from("<H", data, 12)[0]
    if chip != CHIP_ID_ESP32S3:
        fail(name + " is built for chip id " + str(chip) + ", expected 9 (ESP32-S3)")


def partition_table(data):
    entries = {}
    for i in range(0, len(data) - 31, 32):
        magic, ptype, subtype, offset, size = struct.unpack_from("<HBBII", data, i)
        if magic != 0x50AA:
            break
        label = data[i + 12 : i + 28].split(b"\x00")[0].decode("ascii", "replace")
        entries[label] = (ptype, subtype, offset, size)
    return entries


def check_partitions(data):
    table = partition_table(data)
    app = table.get("app0")
    spiffs = table.get("spiffs")
    if not app or (app[2], app[3]) != (APP_OFFSET, APP_SIZE):
        fail("partition table: app0 is not at 0x10000 / 0x300000: " + str(app))
    if not spiffs or (spiffs[2], spiffs[3]) != (SPIFFS_OFFSET, SPIFFS_SIZE):
        fail("partition table: spiffs (settings) moved: " + str(spiffs))


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def read_version():
    src = (ROOT / "src" / "tb_ble_info.cpp").read_text(encoding="utf-8")
    m = re.search("define TB_FIRMWARE_VERSION" + r"\s+" + chr(34) + "([^" + chr(34) + "]+)" + chr(34), src)
    if not m:
        fail("TB_FIRMWARE_VERSION not found in src/tb_ble_info.cpp")
    return m.group(1)


def main():
    tested = "--tested" in sys.argv
    version = read_version()
    firmware_dir = DOCS / "firmware"
    previous = {}
    if (firmware_dir / "version.json").exists():
        previous = json.loads((firmware_dir / "version.json").read_text(encoding="utf-8"))
    files_info = {}
    boot_app0 = BOOT_APP0.read_bytes()
    if len(boot_app0) != 0x2000:    # the otadata partition
        fail("boot_app0.bin has an unexpected size")
    shared = {}
    for lang, env in ENVS.items():
        build = ROOT / ".pio" / "build" / env
        out = firmware_dir / lang
        out.mkdir(parents=True, exist_ok=True)
        parts = []
        for name, offset in PARTS:
            data = boot_app0 if name == "boot_app0.bin" else (build / name).read_bytes()
            if name in ("bootloader.bin", "firmware.bin"):
                check_image(env + "/" + name, data)
            if name == "partitions.bin":
                check_partitions(data)
            if name == "firmware.bin" and len(data) > APP_SIZE:
                fail("firmware.bin does not fit the app partition")
            if name in ("bootloader.bin", "partitions.bin"):
                if shared.setdefault(name, sha256(data)) != sha256(data):
                    fail(name + " differs between languages; they must be the same board setup")
            (out / name).write_bytes(data)
            parts.append({"name": name, "offset": offset, "size": len(data), "sha256": sha256(data)})
        files_info[lang] = parts
        manifest = {
            "name": "TrackBridge firmware (" + ("English" if lang == "en" else "Türkçe") + ")",
            "version": version,
            "new_install_prompt_erase": False,
            "builds": [
                {
                    "chipFamily": "ESP32-S3",
                    "parts": [{"path": "firmware/" + lang + "/" + p["name"], "offset": p["offset"]} for p in parts],
                }
            ],
        }
        write_lf((DOCS / ("manifest-" + lang + ".json")), json.dumps(manifest, indent=2, ensure_ascii=False) + chr(10))
    info = {
        "version": version,
        "built": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d"),
        "tested_on_hardware": bool(tested or previous.get("tested_on_hardware", False)),
        "files": files_info,
    }
    write_lf((firmware_dir / "version.json"), json.dumps(info, indent=2) + chr(10))
    sums = chr(10).join(
        sha256_line for lang in ENVS for sha256_line in (p["sha256"] + "  firmware/" + lang + "/" + p["name"] for p in files_info[lang])
    )
    write_lf((firmware_dir / "SHA256SUMS.txt"), sums + chr(10))

    if "--ewt" in sys.argv:
        src = Path(sys.argv[sys.argv.index("--ewt") + 1])
        dest = DOCS / "vendor" / "esp-web-tools"
        if dest.exists():
            shutil.rmtree(dest)
        shutil.copytree(src / "dist" / "web", dest)
        shutil.copy(src / "LICENSE", dest / "LICENSE")
        pkg = json.loads((src / "package.json").read_text(encoding="utf-8"))
        write_lf((dest / "VERSION.txt"), "esp-web-tools " + pkg["version"] + " (Apache-2.0), unmodified" + chr(10))
    print("OK: version", version, "| tested_on_hardware =", info["tested_on_hardware"])
    for lang in ENVS:
        print(" ", lang, [(p["name"], p["size"]) for p in files_info[lang]])


if __name__ == "__main__":
    main()
