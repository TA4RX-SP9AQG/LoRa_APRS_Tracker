# TrackBridge edition of LoRa APRS Tracker

This branch (`trackbridge`) is a fork of [richonguzman/LoRa_APRS_Tracker](https://github.com/richonguzman/LoRa_APRS_Tracker)
(Ricardo Guzman, CA2RXU, GPL-3.0-or-later) made to work with the [TrackBridge](https://github.com/TA4RX-SP9AQG/TrackBridge)
Android app. All original copyright notices are kept; this edition is independent and not endorsed by the original authors.
Licence: GPL-3.0-or-later, the same as the original. Third-party libraries: see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

**Modification notice (GPL-3.0 section 5a):** this edition was modified from the original LoRa APRS Tracker 4.0.1 in
November 2026 by the TrackBridge project; the changes are listed below and in the git history of this branch. The web
flasher links to the exact source commit of the binaries it offers (GPL-3.0 section 6).

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

### Beacon profiles

Characteristic `...0016` (READ) lists every beacon profile (up to 5), with as much of the comment and label as fits in 500 bytes
(`cc` / `lc` flag cut texts). The `set` keys `"badd":1` (append a profile) and `"bdel":i` (remove profile i; the last one cannot be
removed) change the list. On commit the new list is built from each profile's origin, so Mic-E, status text and long texts of kept
profiles survive, and the remembered active profile is reset to the first one when profiles moved.

### Radio names on the tracker's screen

The stock firmware printed a fixed country name per radio profile *position* (first = EU, second = PL, third = UK, fourth = US) on
the boot screen, the main screen, the Change Frequency menu and after switching profile, so a profile changed from the app kept
its old name. `src/tb_radio.cpp` (PC-tested) now names a profile from its real values: 433.775 MHz SF12 CR4/5 = EU (TR in the
Turkish build: Europe and Turkey share these numbers), 434.855 MHz SF9 CR4/7 = PL, 439.9125 MHz SF12 CR4/5 = UK, 915 MHz SF12
CR4/5 = US (all 125 kHz); anything else is shown as its frequency. The status JSON reports the active radio profile (`"ri"`) and
the control key `"ri"` chooses it (saved on commit, like the menu does). Also fixed on the way: a saved profile number outside
the list is ignored instead of read past the end, and the web page's save follows the real number of beacon and radio profiles.

### TNC modes

Setting `bluetooth.tncMode` (control key `"tnc"`, reported in the status JSON as `"tnc":{"m":set,"e":in effect}`). It decides
what the tracker transmits **by itself**; packets the phone writes over Bluetooth are always transmitted.

| Mode | Beacons, status, telemetry | Answer / store messages | Digipeat | From the tracker's menu |
|---|---|---|---|---|
| 0 Normal | yes | yes (stock behaviour) | if switched on | yes |
| 1 Full TNC | no | no (stored, not answered, while no phone is connected) | no | no ("TNC mode: no TX") |
| 2 Semi TNC | yes | only while no phone is connected (the app does it otherwise) | if switched on | yes |
| 3 Smart | Full while a phone is connected, Semi otherwise; beacons resume 15 s after the phone has gone | | | |

The table is a pure function (`src/tb_tnc.cpp`, tested on a PC); the gates are in `sendBeacon`, `checkStatus`,
`checkReceivedMessage`, `addToOutputBuffer`, `processOutputBuffer` and the keyboard beacon. Old settings files and new
trackers are Normal. "A phone is connected" means any Bluetooth connection: in Smart mode a stranger's phone also
silences the beacons (Bluetooth has no authentication here). In Full mode the app's "Tracker GPS" position source cannot
work, because the tracker sends no beacons to read the position from. The tracker's main screen shows the mode as a four-letter tag at the right end of the
battery row: `FULL`, `SEMI` or `AUTO` (Smart); nothing in Normal mode, and nothing when the row has no room left
(it needs 5 free characters; the Heltec Wireless Tracker row is 26 characters wide).

### Secure link (optional, off by default)

Setting `bluetooth.secure` (app: "Güvenli bağlantı" / "Secure link", needs the PIN and a restart) makes the tracker require
**pairing with a six-digit code shown on its screen** (Bluetooth LE Secure Connections with a passkey, bonding kept) before a
phone can write to the KISS characteristic or the control characteristic, or read the radio-to-phone data. Without it the stock
behaviour stays (open). In this mode a phone counts as "connected" (Smart TNC mode, screen symbol) only after the pairing
succeeded, so a stranger's phone can no longer silence the beacons. Tracker-side files: `src/ble_utils.cpp` (security
settings, characteristic flags, callbacks), `src/tb_ble_control.cpp` (code generation and screen).
**Not tested on a real tracker yet.** If a phone cannot pair, remove the old pairing in the phone's Bluetooth settings and
try again; if the setting must be undone without a connection, flash with "Erase device". The iOS aprs.fi app and other
generic BLE KISS clients do not support passkey pairing from this firmware's point of view as tested; leave the option off for them.

### Known limits

* **The KISS characteristic has no authentication unless the Secure link option is switched on** (as in the original firmware):
  anyone within Bluetooth range can make a tracker with Bluetooth on transmit. With the default (Bluetooth on from the start)
  that is also true for a fresh tracker. Switch on "Secure link" once the tracker works (see above).
* The PIN travels unencrypted over the air once; it is single use, so it cannot be replayed, but someone injecting packets into an
  open session with special equipment is not stopped.
* The settings file is saved through a temporary file (`/tracker_conf.tmp`) that replaces it only after it reads back as valid
  JSON, and a save cut off by a power loss is cleaned up or restored at the next start. This is tested only by reading the code
  and building, not by cutting power on a real tracker. The flasher's "Erase device" option and the automatic first-start repair
  recover from a broken file system.
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
