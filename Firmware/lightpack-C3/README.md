# lightpack-C3 firmware — Lux et Portabilis V2

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--C3-orange.svg)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)
[![Release](https://img.shields.io/badge/Pre--release-V0.4--pre-yellow.svg)](releases/V0.4-pre/)

Firmware for the **Lux et Portabilis V2** board (`Hardware/lux-et-portabilis-V2/`): an
ESP32-C3 SuperMini driving one addressable LED strip, a status pixel, an MPU6050
accelerometer, a battery latch and a servo/analog breakout — with a browser UI on the
LAN, two independent OTA paths, and a complete no-phone control surface on the module's
BOOT switch.

- **Board:** Lux et Portabilis V2 (ESP32-C3 SuperMini, 4 MB flash)
- **Image:** `C3-USB` / `C3-OTA` in `platformio.ini`
- **Install it:** [`install/`](../../install/) — browser flasher, nothing to install
- **Downloads:** [`releases/`](releases/) — prebuilt binaries, no toolchain needed
- **Version history:** [`CHANGELOG.md`](CHANGELOG.md)
- **Not this board?** The Devkit V1 firmware (ESP32-WROOM) lives in `Firmware/lightpack/`.

> **Pre-release status.** This is a bench-verified pre-release, not a 1.0. The
> outstanding hardware unknowns are listed under [Known limitations](#known-limitations),
> and the most load-bearing one — the touch pad input — is **not usable on the current
> bench board at all**. Everything user-facing runs off the BOOT switch instead.

---

## 1. Download and flash

Prebuilt, already-compiled images are committed in [`releases/V0.4-pre/`](releases/V0.4-pre/):

| File | Offset | Purpose |
|---|---|---|
| `lightpack-C3-V0.4-pre-factory.bin` | `0x0` | **Everything in one file** — bootloader, partition table, boot selector and app |
| `firmware.bin` | `0x10000` | The app image alone — this is the file the web page and ArduinoOTA expect |
| `bootloader.bin` | `0x0` | Second-stage bootloader |
| `partitions.bin` | `0x8000` | Custom partition table (see §7) |
| `boot_app0.bin` | `0xE000` | OTA selector; makes the bootloader pick `app0` |

### Option A — browser installer (recommended for a fresh board)

Open [`install/`](../../install/) in **Chrome or Edge on a desktop**, plug the board in, click
**Install V0.4-pre**, pick the port. Nothing to install on the computer: no Python, no esptool, no
USB-serial driver — the C3's USB is native. The page diagnoses its own prerequisites (wrong
browser, `file://`, no secure context) and links the command-line fallback when it cannot work.

The page reads its image from [`releases/V0.4-pre/manifest.json`](releases/V0.4-pre/manifest.json).
To run it locally, serve the **repository root** and open `/install/`:

```bash
python -m http.server 8000      # then http://localhost:8000/install/
```

### Option B — one command over USB

```bash
esptool.py --chip esp32c3 --port COM6 --baud 460800 write_flash 0x0 lightpack-C3-V0.4-pre-factory.bin
```

`COM6` is the port the SuperMini enumerated as; on macOS/Linux it is
`/dev/cu.usbmodem*` or `/dev/ttyACM0`. Put the board in download mode by holding **BOOT**
while tapping **RESET** if it does not answer.

### Option C — from the device's own page, no cable

With the board already running any version of this firmware, open `http://lightpack.local`,
go to the firmware section and upload `firmware.bin`. It is written to the **inactive** OTA
slot and the board reboots into it; a failed or truncated upload leaves the running image
untouched.

### Option D — individual images

```bash
esptool.py --chip esp32c3 --port COM6 write_flash \
  0x0     bootloader.bin \
  0x8000  partitions.bin \
  0xe000  boot_app0.bin \
  0x10000 firmware.bin
```

> **If the board was last updated over the air, clear the OTA selector first:**
> `esptool.py --chip esp32c3 --port COM6 erase_region 0xE000 0x2000`
> The V0.4 partition table moves `app1` from `0x150000` to `0x200000`. A normal PlatformIO
> upload rewrites `boot_app0.bin` and takes care of this by itself; a raw `write_flash` of
> the app does not. Details in the header of `partitions_lightpack.csv`.

Checksums for every file are in [`releases/V0.4-pre/FLASHING.md`](releases/V0.4-pre/FLASHING.md).

---

## 2. On-device controls

The whole user interface is the **BOOT switch on the SuperMini module (GPIO9)**. It is on the
module itself, so it keeps working no matter what happens to the carrier board.

| Gesture | Action |
|---|---|
| 1 click | next LED mode |
| 2 clicks | previous LED mode |
| 5 clicks | cut the battery latch — power down |
| 3 s hold | toggle the radio: offline mode on / off |
| 6 s hold | reboot into the WiFi configuration hotspot |
| 10 s hold | recovery wipe: erase saved networks and the page login |

How the timing behaves, because it is the part users notice:

- **A click is not the same as a short hold.** A press longer than 600 ms is never counted
  as a click, so a 1.5 s press cannot silently become a mode change.
- **Click counts resolve 400 ms after the finger lifts.** Telling one click from two, and
  two from five, is impossible on release alone, so every click pays that window and the
  mode changes 400 ms late. A double-tap that is still inside 400 ms behaves as one gesture.
- **Holds act on release, and only one fires.** The 6 s portal and the 3 s radio toggle are
  exclusive by construction: the longest applicable threshold wins, so a portal hold never
  also toggles the radio. The 10 s recovery hold shows an amber countdown from 3 s.
- **Holding BOOT through a reset is still the ROM's download mode**, which no sketch can
  see or use.

### The touch pad (GPIO3) — hardware damaged on the bench board

`TOUCH1` takes an external momentary touch module (pin 1 = `+BATT`, pin 2 = signal,
pin 3 = GND). The firmware still classifies it and still reports it on the page — 1 s hold =
next mode, 5 s hold = power down — but **on the current bench board it never fires**: the pad
measures a clean 3.3 V while the pin reads LOW and latches no edge. Board power button `BT1`
puts `+BATT` (~4.2 V) onto a 3.3 V GPIO through `R12`, and `D6` does not clamp it.

Consequences worth knowing before wiring one:

- The firmware assumes a **momentary, active-HIGH** module (`INPUT_PULLUP`/`INPUT_PULLDOWN`
  is set from the `active_high` flag, and the ISR edge follows the same flag). An active-LOW
  or latching module reads as dead or as one endless press — which powers the board off at 5 s.
- `tools/button_probe/` measures which kind of module is plugged in without flashing the
  application, and flipping the polarity is one argument to `button_input_configure()`.
- The schematic now carries `D7` (BZV55B2V7, 2.7 V) as a clamp for this node. Note that the
  C3's VIH is ~2.48 V, so **2.7 V is marginal for a clamp** — 3.3 V or 3.6 V is the value
  this design actually wants. See `CHANGELOG.md`.

---

## 3. What it does

### LED modes

Five modes, ids deliberately gapped so ids never double as positions:

| Pos | id | key | Description |
|---:|---:|---|---|
| 1 | 1 | `solid` | Solid colour at the configured palette colour |
| 2 | 2 | `flame` | Stepped flame |
| 3 | 4 | `fade` | Accelerometer-driven fade |
| 4 | 5 | `whip` | Pulse whip |
| 5 | 6 | `rainbow` | Rainbow flow |

Every tunable (speed, length, period, peak, offset, min/max interval, palette colour,
brightness, LED count) is a field on the web page; the device itself only selects modes.

> **V0.3 carried a sixth mode, `majora` (id 3), which is retired.** Ids were left at their old
> numbers rather than renumbered, so a configuration saved by an older build is not silently
> reinterpreted as a different effect. A stored id with no registry entry is not an error: the
> configuration load falls back to the first entry, so an upgraded board boots lit (on `solid`)
> rather than dark.

### Storage — four independent kinds

| What | Where | Survives |
|---|---|---|
| Live configuration (mode, palette, every tunable, LED count) | NVS namespace | OTA, power cycle, reflash |
| 5 preset slots, each a full named snapshot with a format version | NVS blobs | ditto |
| 2 WiFi credential slots | NVS | ditto |
| Page login (optional) | NVS | ditto |

`lightpack-presets.json` in this directory is an **export** of known-good preset values for
the page's import function, not something the firmware reads at build time.

### Connectivity and update paths

- **Captive-portal provisioning.** On first boot, or when no saved network answers, the board
  raises a temporary hotspot — SSID `Lightpack-Devkit`, 60 s portal window — and serves the
  configuration page from it.
- **LAN name.** `lightpack.local` (mDNS), with the DHCP hostname `lightpack` set on every boot.
- **Offline mode.** The radio is switched off entirely; everything on the device keeps working,
  and the 3 s hold brings it back.
- **ArduinoOTA.** `pio run -e C3-OTA -t upload` pushes over the network to `lightpack.local`.
- **Browser OTA.** Upload `firmware.bin` from the page (§1, Option B).

### Diagnostics that ship with it

Boot reason report, unloaded rail voltage at boot + minimum rail through the strip self-test,
status-pixel network blink (green/cyan/red), amber recovery countdown, I²C bus scan with
address report, MPU6050 auto-address probe (0x68/0x69), impact detector, and periodic serial
telemetry at 115200 baud.

---

## 4. Web interface and API

The page is served on port 80. Every route below is unauthenticated unless a page login has
been set.

| Route | Methods | Purpose |
|---|---|---|
| `/` | GET | Configuration page |
| `/api/state` | GET | Live state (mode, readings, rail, uptime) |
| `/api/buttons` | GET | Live state of both switches, including the damaged pad |
| `/api/modes` | GET | Mode registry |
| `/api/mode` | POST | Select a mode |
| `/api/config` | POST | Write the full configuration |
| `/api/auth` | GET / POST | Read or set the page login |
| `/api/wifi` | GET / POST | Saved networks |
| `/api/wifi/scan` | POST / GET | Start a scan, then poll results |
| `/api/presets` | GET | List the 5 preset slots |
| `/api/preset` | GET / POST | Read or write one slot; import/export |
| `/api/update` | GET / POST | OTA target info; then the `firmware.bin` upload |

> **Security, stated plainly.** The page has no authentication by default and there is no
> TLS: anyone who can reach port 80 can change the lights **and install firmware**. On a
> trusted LAN that is the design; exposed to a wider network it is remote code execution.
> Set the page login if that matters to you.

---

## 5. Building from source

```bash
cd Firmware/lightpack-C3
pio run -e C3-USB                 # build; output in .pio/build/C3-USB/
pio run -e C3-USB -t upload       # over USB
pio run -e C3-OTA -t upload       # over the network
pio device monitor                # serial, 115200
```

Reference build for V0.4-pre (PlatformIO Core 6.1.19, `espressif32@6.11.0`,
`framework-arduinoespressif32` 3.0.1, esptool 4.5.1):

```
RAM:   12.5% (40,964 / 327,680 bytes)
Flash: 61.9% (1,256,946 / 2,031,616 bytes)
```

`-fno-exceptions -fno-asynchronous-unwind-tables` are set deliberately in `platformio.ini`:
there is no `throw`/`try`/`catch` anywhere in this firmware, and the unwind tables were ~49 KB
of dead weight in a partition that used to be 98% full.

### Diagnostic projects (not environments of this one)

```bash
pio run -d tools/button_probe -t upload --upload-port COM6
```

Measures the touch module's polarity. It is a separate PlatformIO project on purpose: a
`src_dir` override inside this project is silently ignored and would flash the **application**
while claiming to be a diagnostic.

---

## 6. Pinout

| Function | GPIO | Notes |
|---|---|---|
| LED strip data | 1 | `strip_pin` |
| Servo | 2 | Unrouted on this build (`#madhephaestus/ESP32Servo` commented out) |
| Touch module (`Button_GPIO`) | 3 | `TOUCH1` signal; shared with `BT1` via `R12`, clamped by `D7` (schematic) |
| Battery latch enable | 4 | Driven HIGH at boot; LOW cuts the latch |
| Analog channel 0 | 0 | ADC |
| Free GPIO A / B | 5 / 6 | `PORT3` |
| I²C SDA / SCL | 7 / 8 | 100 kHz, `MPU6050` at 0x68/0x69 |
| BOOT switch | 9 | **The entire user interface**; also the ROM's download-mode strap |
| Status pixel | 10 | `Status_LED`, WS2812B at D2 (confirmed working) |
| Serial0 TX / RX | 20 / 21 | Unrouted |

---

## 7. Partition table

`partitions_lightpack.csv` replaces the Arduino core's `default.csv`, which reserved 1.375 MB
for a SPIFFS partition this firmware never mounts (`LittleFS` is included but nothing calls it).

| Name | Type | SubType | Offset | Size |
|---|---|---|---:|---:|
| nvs | data | nvs | 0x9000 | 0x5000 |
| otadata | data | ota | 0xE000 | 0x2000 |
| app0 | app | ota_0 | 0x10000 | 0x1F0000 |
| app1 | app | ota_1 | 0x200000 | 0x1F0000 |
| coredump | data | coredump | 0x3F0000 | 0x10000 |

App headroom goes from ~68 KB to ~770 KB per slot. **NVS keeps its offset and size on
purpose**, so a board reflashed with this table keeps its mode, every tunable, its presets,
its saved networks and its page login — the reflash costs a cable, not a configuration.

---

## 8. Known limitations

1. **The touch pad is dead on the bench board** (§2). Design intent and the hardware cause are
   documented in `docs/pre-release-plan.md`; the firmware's inputs live on GPIO9 instead.
2. **The zener on the schematic is the wrong value.** `D7` was added as `BZV55B2V7` (2.7 V);
   the C3 needs its clamp above VIH (~2.48 V), so 3.3 V or 3.6 V is correct. The `1N4148W` at
   `D6` was never a clamp either — its far end lands on the latch-gate node, not a 3.3 V rail.
3. **The schematic change has no PCB yet.** `lux-et-portabilis-V2.kicad_pcb` and the
   production files were not regenerated for `D7`, so do not order boards expecting it.
4. **No OTA rollback.** The Arduino core never marks an app valid and
   `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is off, so an image that boots but cannot join WiFi
   must be recovered over USB. Keep the last-known-good `firmware.bin`.
5. **The page is unauthenticated by default** (§4).
6. **`Adafruit SSD1306` / `Adafruit GFX` are in `lib_deps` but no display service exists.**
   `display_started` in `shared/variables.h` is never set by anything.
7. **Announce brightness is unverified behind a case diffuser** — the one open item in
   `docs/pre-release-plan.md` §8.

---

## 9. License

The firmware sources, this documentation and the release images are released under **0BSD**
([`../../LICENSE`](../../LICENSE)): no conditions, no attribution, commercial use included.

That covers *this project's* code only. The images also link the Arduino-ESP32 core
(LGPL-2.1-or-later) and Adafruit NeoPixel (LGPL-3.0-or-later), plus MIT/BSD/Apache libraries — those
terms travel with the binary. Before redistributing a `.bin`, read
[`THIRD-PARTY-NOTICES.md`](../../THIRD-PARTY-NOTICES.md); each release folder carries its own copy
next to the images for exactly that reason.

## 10. Layout

```
lightpack-C3/
├── platformio.ini              two envs: C3-USB, C3-OTA
├── partitions_lightpack.csv    custom table, with the reasoning in its header
├── lightpack-presets.json      export of known-good preset values (import on the page)
├── README.md                   this file
├── CHANGELOG.md                version history; V0.4-pre entry documents the release
├── releases/
│   ├── README.md               release index + how to cut a release
│   ├── V0.4-pre/               prebuilt binaries + FLASHING.md + manifest.json + checksums
│   └── V0.3/                   lightpack_V03-source.zip (sources only)
├── docs/
│   └── pre-release-plan.md     feature list, menu design, NVS cost analysis, open items
├── tools/button_probe/         separate project: measures touch-module polarity
└── src/
    ├── main.cpp                setup() / loop()
    ├── components/ESP32/
    │   ├── setup.h             bring-up, strip self-test, rail report
    │   └── features/
    │       ├── update_firmware_ota.h    ArduinoOTA
    │       └── update_firmware_web.h    browser upload to the inactive slot
    ├── services/
    │   ├── led_scripts.h       mode registry, effects, strip driver
    │   ├── webpage.h           page + JSON API
    │   ├── readings.h          switches, ADC, rail
    │   ├── presets.h           NVS preset slots, versioned format
    │   ├── wifi_settings.h     portal, credentials, offline mode, mDNS
    │   ├── gyros.h             MPU6050, impact detector
    │   ├── i2c_scan.h          bus scan
    │   └── serial_outputs.h    boot report, telemetry
    └── shared/                 pinout, variables, dependencies, library objects
```

Folder names are under `Firmware/lightpack-C3/` except at the top of the list. Two things live
outside it: `install/` at the **repository root** (the browser flasher, which consumes
`releases/*/manifest.json`) and `docs/pre-release-plan.md`'s design notes for the unbuilt menu.
