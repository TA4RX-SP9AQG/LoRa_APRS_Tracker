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
| **Changing settings from the app** | `src/tb_control.cpp`, `src/tb_ble_control.cpp` | The app can edit the beacon profiles, path, radio parameters and options, after a PIN shown on the tracker's own screen. Fresh settings start with Bluetooth on, so the first set-up can also be done from the app. |
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

### Changing settings from the app

Two more characteristics of the same service: `...0014` (control, WRITE) takes JSON commands, `...0015` (result, READ) holds the
answer to the last one (matched by the command's `id`).

1. `{"id":1,"op":"pin"}`: the tracker shows a random 6-digit code on its screen (valid 60 s, one PIN per 10 s and at most 3 per
   5 minutes, bound to the asking connection).
2. `{"id":2,"op":"auth","pin":"123456"}`: opens a 5 minute session (sliding) for that connection. Three wrong tries over any
   number of PINs lock everything for 60 s, doubling with every further lockout up to 30 min; a success resets it. The session
   ends when the connection drops.
3. `{"id":3,"op":"set", ...}`: one section per command (`b` beacon profile, `l` radio profile, `path`, `disp`, `bat`, `o`, `bt` name,
   `tel`). Everything is validated (callsign, path, symbol, comment characters, radio ranges of the board's chip ...) and staged in a
   copy. Unknown keys are refused, so passwords, e-mail, Winlink data and the Bluetooth/KISS switches cannot be reached.
4. `{"id":4,"op":"commit"}`: saves only the fields that differ from the current settings (values the app never touched keep their exact
   stored form), checks that the file can be read back, and restarts the tracker. `{"op":"discard"}` drops staged changes.

The rules (`tb_control.cpp`) have no Arduino code and are tested on a PC with ~3200 checks (`tools/tb_test`).

### Known limits

* **The KISS characteristic has no authentication** (as in the original firmware): anyone within Bluetooth range can make a
  tracker with Bluetooth on transmit. With the new default (Bluetooth on from the start) that is also true for a fresh tracker.
  Gating KISS transmit behind the same PIN session, or LE Secure Connections with a passkey, is the proper fix and not done yet.
* The PIN travels unencrypted over the air once; it is single use, so it cannot be replayed, but someone injecting packets into an
  open session with special equipment is not stopped.
* The settings file is not written atomically; a power cut during the save can lose it (the tracker then starts with the default
  settings). The flasher's "Erase device" option and the automatic first-start repair recover from a broken file system.
* A radio setting the chip refuses stops the tracker at start-up. The ranges are limited to what the board's chip accepts, but
  nothing is tested on a real tracker yet.

## Tests and checks

* `python tools/tb_test/run.py` builds the JSON code on the PC and checks ~1200 payloads (including random hostile input and
  guard bytes against buffer overruns): valid JSON, within 500 bytes, no private setting names. Needs a host C++ compiler
  (`uv tool install ziglang` provides one).
* English build equivalence: when the on-screen texts were wrapped for translation, the English firmware image differed from the
  build before only in build timestamps (84 bytes).
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
