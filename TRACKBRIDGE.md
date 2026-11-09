# TrackBridge edition of LoRa APRS Tracker

This branch (`trackbridge`) is a fork of [richonguzman/LoRa_APRS_Tracker](https://github.com/richonguzman/LoRa_APRS_Tracker)
(Ricardo Guzman, CA2RXU, GPL-3.0-or-later) made to work with the [TrackBridge](https://github.com/TA4RX-SP9AQG/TrackBridge)
Android app. All original copyright notices are kept; this edition is independent and not endorsed by the original authors.
Licence: GPL-3.0-or-later, the same as the original.

## What is different

| Change | Where | Effect on the tracker |
|---|---|---|
| **Device info Bluetooth service** (read only) | `src/tb_info.cpp`, `src/tb_ble_info.cpp`, hooks in `ble_utils` and the main loop | The app can read battery, GPS status, a settings summary and the newest stored messages. Nothing can be written through it. |
| **Turkish build** | `include/tb_lang.h`, `TB_T("English", "Turkce")` in `src/`, `tools/tb_translate/` | Environment `heltec_wireless_tracker_tr`: Turkish on the tracker screen (ASCII only, the font has no Turkish letters) and a Turkish web configuration page. The English build is the same program as before. |
| **Message storage fixes** | `src/msg_utils.cpp` | A received text can no longer forge extra lines in the stored messages; the message counter goes back to 0 when the file is deleted. |
| **Web flasher** | `docs/`, `tools/build_flasher.py` | Installs either language from a browser (Chrome/Edge, Web Serial). |

### Device info service

Service `00000010-ba2a-46c9-ae49-01b0961f68bb` (the KISS service is unchanged), three READ-only characteristics holding
compact JSON (protocol version `v` = 1, at most 500 bytes each):

| UUID | Content |
|---|---|
| `…0011` | status: firmware, uptime, battery (V, %, current), GPS (fix, satellites, HDOP, speed, altitude, age), last transmission, active callsign, radio parameters, free memory |
| `…0012` | settings summary: beacon profiles (callsign, symbol, comment, SmartBeaconing), path, radio parameters, display, battery, Bluetooth name |
| `…0013` | newest 8 stored APRS messages (sender, text), total count |

What is **never** sent: WiFi AP password, Winlink password, e-mail, the exact GPS position. The JSON is built from
whitelisted plain fields (`include/tb_info.h`) in the main loop, never inside a Bluetooth callback. Like the existing KISS
service it needs no pairing, so anyone within Bluetooth range can read it, including the stored messages.

## Tests and checks

* `python tools/tb_test/run.py` builds the JSON code on the PC and checks ~1200 payloads (including random hostile input and
  guard bytes against buffer overruns): valid JSON, within 500 bytes, no private setting names. Needs a host C++ compiler
  (`uv tool install ziglang` provides one).
* English build equivalence: after wrapping the texts, the English firmware image differs from the previous build only in
  build timestamps (84 bytes).
* Not done: running on a real tracker. `docs/firmware/version.json` says `"tested_on_hardware": false` until
  `python tools/build_flasher.py --tested` is run after a real-device test.

## Building and publishing the flasher

```
pio run -e heltec_wireless_tracker -e heltec_wireless_tracker_tr
python tools/build_flasher.py            # checks the images, writes docs/firmware and docs/manifest-*.json
```

The checks refuse images for another chip or a changed partition table (the settings partition must stay where the
original firmware keeps it). Publish `docs/` with GitHub Pages (branch `trackbridge`, folder `/docs`). The page uses
[ESP Web Tools](https://github.com/esphome/esp-web-tools) (Apache-2.0), copied unmodified into `docs/vendor/`, so the page
loads nothing from third-party servers.

## Merging upstream

The changes are additive: new files plus small hooks (`ble_utils`, the main loop, `msg_utils`, `compress.py`) and
`TB_T(...)` wrappers that `python tools/tb_translate/apply.py --write` can re-apply after a merge.
