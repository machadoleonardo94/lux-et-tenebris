# Third-party notices

This project is licensed **0BSD** (see [`LICENSE`](LICENSE)) — public-domain-equivalent,
no attribution required, commercial use, modification, redistribution, all fine.

This file exists because the **compiled firmware images** in
[`Firmware/lightpack-C3/releases/`](Firmware/lightpack-C3/releases/) link third-party libraries
whose licenses are not 0BSD and whose terms travel with the binary, no matter what this project's
own license says. It reproduces their notices and says what a redistributor has to keep.

Versions are the ones pinned in
[`Firmware/lightpack-C3/platformio.ini`](Firmware/lightpack-C3/platformio.ini); a future release
should be assumed to have the same components unless its own notices say otherwise.

---

## 1. What this project owns

Original work — firmware sources, the browser installer, schematics, PCB files, documentation,
preset exports and simulation files — is licensed **0BSD**. Do with it what you like.

## 2. What is linked into the firmware image

| Component | Version | License | Copyright |
|---|---|---|---|
| Arduino-ESP32 core — `WiFi`, `WebServer`, `ArduinoOTA`, `Update`, `Preferences`, `Wire`, `SPI`, `ESPmDNS`, `DNSServer`, `LittleFS` | 3.0.1 (`framework-arduinoespressif32`) | **LGPL-2.1-or-later** | Arduino Team, Espressif Systems |
| Adafruit NeoPixel | 1.15.5 | **LGPL-3.0-or-later** | Adafruit Industries (written by Phil Burgess, with PJRC, Michael Miller and contributors) |
| Adafruit GFX Library | 1.12.6 | BSD-3-Clause (Adafruit) | © 2012 Adafruit Industries |
| Adafruit SSD1306 | 2.5.17 | BSD-3-Clause (Adafruit) | © 2012 Adafruit Industries |
| Adafruit ADS1X15 | 2.6.2 | BSD-3-Clause (Adafruit) | © 2012 Adafruit Industries |
| Adafruit MPU6050 | 2.2.9 | BSD-3-Clause (Adafruit) | © 2019 Bryan Siepert for Adafruit Industries |
| Adafruit BusIO | 1.17.4 | MIT | © 2017 Adafruit Industries |
| Adafruit Unified Sensor | 1.1.15 | Apache-2.0 | Adafruit Industries |
| WiFiManager | 2.0.17 | MIT | © 2015 tzapu |
| ESP-IDF and `esp32-arduino-libs` (includes Espressif's prebuilt Wi-Fi/PHY/BLE libraries) | 3.0.1 | Apache-2.0, plus Espressif binary blobs under Espressif's own licence | © Espressif Systems |
| GNU RISC-V toolchain runtime | as supplied by the platform | GPL-3.0 **with the GCC Runtime Library Exception** | © Free Software Foundation / GCC contributors |

The toolchain's exception means compiled output carries no GPL obligation — the other rows are the
ones that matter.

`Adafruit SSD1306`, `Adafruit GFX Library` and `LittleFS` are declared build dependencies but are
not used by this firmware; the linker drops what nothing references. They are listed anyway,
because a future release may start using them and this file should not have to be re-audited from
scratch.

## 3. The LGPL components — read this before redistributing a `.bin`

**Notice.** The firmware image incorporates the **Arduino-ESP32 core (LGPL-2.1-or-later)** and
**Adafruit NeoPixel (LGPL-3.0-or-later)**. Both libraries, and their use in this image, are covered
by those licenses — not by this project's 0BSD license.

**No modifications.** Neither library was patched, renamed or configured in any way; they are the
unmodified upstream releases at the versions listed above, fetched by PlatformIO from the library
registry.

**Full texts.** [`LICENSES/LGPL-3.0.txt`](LICENSES/LGPL-3.0.txt) is included verbatim. The
Arduino-ESP32 package does not ship the LGPL-2.1 text itself; the canonical copy is at
<https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html>.

**Relinking.** The LGPL's recombine/relink condition is satisfied by publishing the *complete*
application source and build recipe, which is the point of this repository: clone it, replace the
library in `.pio/libdeps/` (or override `lib_deps`) with your modified copy, and run
`pio run -e C3-USB`. That produces a new image from a modified library — no object files need to be
shipped.

**What that means for you:** ship the `.bin` if you like, commercially too. Keep this file with it,
keep the LGPL parts under LGPL, and point recipients at this repository (or wherever you got the
source) so they can relink.

## 4. The permissive components

MIT, BSD-3-Clause and Apache-2.0 require their copyright notice and license text to travel with
binary redistribution. Their texts are included verbatim:

| License | File | Covers |
|---|---|---|
| MIT | [`LICENSES/MIT.txt`](LICENSES/MIT.txt) | WiFiManager, Adafruit BusIO |
| BSD-3-Clause | [`LICENSES/BSD-3-Clause-Adafruit.txt`](LICENSES/BSD-3-Clause-Adafruit.txt) | Adafruit GFX, SSD1306, ADS1X15, MPU6050 |
| Apache-2.0 | [`LICENSES/Apache-2.0.txt`](LICENSES/Apache-2.0.txt) | Adafruit Unified Sensor, ESP-IDF |

None of these libraries were modified.

## 5. The browser installer

[`install/`](install/) loads **ESP Web Tools 10.4.0** (Apache-2.0, ESPHome / Open Home Foundation)
from unpkg at runtime, together with its own dependencies `esptool-js`, `lit` and `@material/web`.
**Nothing is redistributed from this repository**, so no notice obligation attaches — the footer
attribution on the page is a courtesy, not a requirement. This is why the component is not vendored.

If you ever vendor it for offline use, Apache-2.0 then applies: ship its `LICENSE` and `NOTICE`,
and state any modifications you made. `install/README.md` repeats this.

## 6. Hardware and non-code assets

- **KiCad libraries.** The schematic uses symbols from KiCad's official libraries (for example
  `Diode:BZV55B2V7`, `power:GND`). Those libraries are **CC-BY-SA-4.0 with a design exception**:
  using them in a design imposes nothing on the design. Library files themselves are not
  redistributed here — only the symbol definitions KiCad caches inside the `.kicad_sch`, which is
  normal behaviour for every KiCad project.
- **Production files** (gerbers, BOM, positions) are generated from this project's own design.
- **`Media/*.mp4` and `Simulations/*`** are the author's own recordings and simulation runs.

## 7. If you redistribute a firmware image

- ✅ Keep this file (or its content) alongside the `.bin`.
- ✅ Say where the source is, so recipients can relink the LGPL parts.
- ✅ Keep the LGPL parts under LGPL — you cannot relicense them to 0BSD or anything else.
- ⚠️ No rollback means a bad image needs USB recovery: that is an operational note, not a license
  one, but it belongs in the same conversation as "who supports this device".

---

*This file is a practical audit of the licenses actually present in this repository and its
dependencies, written by the maintainers. It is not legal advice.*
