# Third-party notices

The TrackBridge edition of LoRa APRS Tracker is licensed **GPL-3.0-or-later** (see [LICENSE](LICENSE)). It is built from
the original [LoRa APRS Tracker](https://github.com/richonguzman/LoRa_APRS_Tracker) by Ricardo Guzman (CA2RXU) and
contributors, with the libraries below. Each library keeps its own licence and copyright notice; their full texts are in
the library packages that PlatformIO downloads at build time (see `common_settings.ini` for the exact versions).

| Library | Licence |
|---|---|
| APRSPacketLib (Ricardo Guzman, CA2RXU) | GPL-3.0-or-later |
| ArduinoJson (Benoit Blanchon) | MIT |
| RadioLib (Jan Gromes) | MIT |
| NimBLE-Arduino (h2zero) | Apache-2.0 |
| TFT_eSPI (Bodmer) | MIT |
| Adafruit BusIO, BMP280, GFX, SSD1306, BME280, BME680 | MIT / BSD (see each package) |
| Adafruit Unified Sensor | Apache-2.0 |
| OneButton (Matthias Hertel) | BSD-3-Clause |
| esp-logger (peterus) | MIT |
| AsyncTCP, ESPAsyncWebServer (ESP32Async) | LGPL-3.0 |
| TinyGPSPlus (Mikal Hart) | LGPL-2.1-or-later |
| Time (Paul Stoffregen) | LGPL-2.1-or-later |
| Arduino-ESP32 / ESP-IDF (Espressif), incl. the bootloader and `boot_app0` images the web flasher writes | Apache-2.0 / LGPL-2.1 (see the Arduino-ESP32 repository) |
| ESP Web Tools (used by the flasher page, copied unmodified to `docs/vendor/esp-web-tools`) | Apache-2.0 (`docs/vendor/esp-web-tools/LICENSE`) |

The LGPL libraries are combined into a GPL-3.0 work whose complete source is published, so the LGPL conditions (source
availability, the right to replace the library) are met. The flasher page links to the exact source commit of the binaries
it offers.

This list was compiled from the licence files of the libraries at the versions pinned in `common_settings.ini`. If you
find a mistake, please open an issue.
