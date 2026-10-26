# Third-party notices — lightpack-C3 V0.4-pre binaries

This folder exists to be copied wholesale, so the notices that must **accompany** these images are
here too, next to the `.bin` files they describe.

The images (`lightpack-C3-V0.4-pre-factory.bin`, `firmware.bin`) link third-party libraries whose
license terms travel with the binary, regardless of this project's own license (0BSD). The full
audit — component versions, per-library copyright lines, and what a redistributor must keep — is
[`../../../../THIRD-PARTY-NOTICES.md`](../../../../THIRD-PARTY-NOTICES.md). The essentials:

## LGPL notice

The firmware **incorporates the Arduino-ESP32 core (LGPL-2.1-or-later) and Adafruit NeoPixel
(LGPL-3.0-or-later)**, unmodified upstream releases:
`framework-arduinoespressif32` 3.0.1 and `Adafruit NeoPixel` 1.15.5. Both libraries, and their use
in these images, are covered by those licenses and not by this project's 0BSD license.

- Full text of LGPL-3.0: [`../../../../LICENSES/LGPL-3.0.txt`](../../../../LICENSES/LGPL-3.0.txt)
- LGPL-2.1 (the Arduino package ships no copy of its own):
  <https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>
- **Relinking:** the complete application source and `platformio.ini` are in this repository. Replace
  a library in `.pio/libdeps/` (or override `lib_deps`) with a modified copy and run
  `pio run -e C3-USB` to produce a new image from it.
- You may redistribute these images, commercially included, provided the LGPL parts stay LGPL and
  recipients can reach the source to relink.

## Other bundled components

| Component | Version | License | Copyright |
|---|---|---|---|
| Adafruit GFX Library | 1.12.6 | BSD-3-Clause | © 2012 Adafruit Industries |
| Adafruit SSD1306 | 2.5.17 | BSD-3-Clause | © 2012 Adafruit Industries |
| Adafruit ADS1X15 | 2.6.2 | BSD-3-Clause | © 2012 Adafruit Industries |
| Adafruit MPU6050 | 2.2.9 | BSD-3-Clause | © 2019 Bryan Siepert for Adafruit Industries |
| Adafruit BusIO | 1.17.4 | MIT | © 2017 Adafruit Industries |
| Adafruit Unified Sensor | 1.1.15 | Apache-2.0 | Adafruit Industries |
| WiFiManager | 2.0.17 | MIT | © 2015 tzapu |
| ESP-IDF / `esp32-arduino-libs` | 3.0.1 | Apache-2.0 + Espressif blobs | © Espressif Systems |

License texts: [`MIT`](../../../../LICENSES/MIT.txt) ·
[`BSD-3-Clause`](../../../../LICENSES/BSD-3-Clause-Adafruit.txt) ·
[`Apache-2.0`](../../../../LICENSES/Apache-2.0.txt). No library was modified.
