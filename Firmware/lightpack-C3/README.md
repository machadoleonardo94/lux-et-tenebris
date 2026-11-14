# lightpack-C3 firmware — Lux et Portabilis V2

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--C3-orange.svg)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue.svg)](https://www.arduino.cc/)
[![Release](https://img.shields.io/badge/Pre--release-V0.4--pre-yellow.svg)](releases/V0.4-pre/)

Firmware for the **Lux et Portabilis V2** board (`Hardware/lux-et-portabilis-V2/`): an
ESP32-C3 SuperMini driving one addressable LED strip, a status pixel, an MPU6050
accelerometer, a battery latch and a servo/analog breakout — with a browser UI on the
LAN, two independent OTA paths, and a complete no-phone control surface on the touch pad
and the module's BOOT switch.

- **Board:** Lux et Portabilis V2 (ESP32-C3 SuperMini, 4 MB flash)
- **Image:** `C3-USB` / `C3-OTA` in `platformio.ini`
- **Install it:** [`install/`](../../install/) — browser flasher, nothing to install
- **Downloads:** [`releases/`](releases/) — prebuilt binaries, no toolchain needed
- **Version history:** [`CHANGELOG.md`](CHANGELOG.md)
- **Not this board?** The Devkit V1 firmware (ESP32-WROOM) lives in `Firmware/lightpack/`.

> **Pre-release status.** This is a bench-verified pre-release, not a 1.0. The
> outstanding hardware unknowns are listed under [Known limitations](#known-limitations),
> and the most load-bearing one — the touch pad input — **was dead on the first bench
> board**. The current unit's touch module works and is now the daily control; the BOOT
> switch remains the fallback and the recovery input.

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

Two switches, with deliberately different jobs. **GPIO3 is the touch pad and the daily
interface.** **GPIO9 is the SuperMini's BOOT switch and the security/recovery interface** —
it lives on the module, so it keeps working no matter what happens to the carrier board,
and it is the fallback if the touch pad fails again.

### Touch pad — GPIO3 (daily)

| Gesture | Action |
|---|---|
| 1 click | next LED mode |
| 2 clicks | previous LED mode |
| 5 clicks | cut the battery latch — power down |
| 3 s hold, release | toggle the radio: offline mode on / off |
| 6 s hold, release | reboot into the WiFi configuration hotspot |

### BOOT switch — GPIO9 (security / recovery / fallback)

| Gesture | Action |
|---|---|
| 1 click | previous LED mode — fallback navigation when the pad is unavailable |
| 5 clicks | cut the battery latch — power down |
| 3 s hold, release | toggle the radio: offline mode on / off |
| 6 s hold, release | reboot into the WiFi configuration hotspot |
| 10 s hold, release | recovery wipe: erase saved networks and the page login |

Repeated BOOT clicks walk the whole registry backwards, so a board with a dead pad still
reaches every mode. The 10 s wipe is deliberately **BOOT-only**: it erases settings, and
it should not sit under the pad a sleeve can brush.

How the timing behaves, because it is the part users notice:

- **A click is not the same as a short hold.** A press longer than 600 ms is never counted
  as a click, so a 1.5 s press cannot silently become a mode change.
- **Click counts resolve 400 ms after the finger lifts.** Telling one click from two, and
  two from five, is impossible on release alone, so every click pays that window and the
  mode changes 400 ms late. A double-tap that is still inside 400 ms behaves as one gesture.
- **Holds act on release, and only one fires.** The 6 s portal and the 3 s radio toggle are
  exclusive by construction: the longest applicable threshold wins, so a portal hold never
  also toggles the radio. The 10 s recovery hold shows an amber countdown from 7 s.
- **Power-off is armed 10 s after boot.** The latch cut is refused during the boot and
  self-test window, so five clicks in the first 10 s do nothing (the refusal is logged).
  This guard is what keeps a boot-time spurious press from cutting power.
- **Power-off only cuts battery power.** With USB connected the latch opens but the board
  stays alive and restarts after the red indication; unplug USB to see it actually shut
  down. The serial log says which happened.
- **Holding BOOT through a reset is still the ROM's download mode**, which no sketch can
  see or use.

### Feedback — the status pixel and the strip

The onboard WS2812B pixel (GPIO10) is the always-visible channel. The strip (GPIO1) gives a
short flash of the same colour so the action can be seen across the room; then the selected
mode resumes. The strip flash is current-limited to a peak of 10/255 per channel, so a
250-pixel white flash is a brief ~0.6 A, not the several amps a full-brightness white would
ask for.

| Action | Status pixel | Strip |
|---|---|---|
| next mode | blue, 600 ms | one blue flash, 140 ms |
| previous mode | cyan, 600 ms | one cyan flash, 140 ms |
| power off | red, held 1 s | red, held 1 s |
| radio on | green, 700 ms | one green flash, 160 ms |
| radio off | red, 700 ms | one red flash, 160 ms |
| config hotspot | white, 400 ms | white, 400 ms |
| recovery wipe | magenta, 1.2 s | magenta, 1.2 s |
| 3 or 4 clicks (no gesture) | dim red, 250 ms | dim red, 80 ms |

While a switch is held, the pixel is a level indicator: **green** on the touch pad or
**purple** on BOOT below 3 s, **amber** from 3 s (release now toggles the radio), **blue**
from 6 s (release now opens the hotspot). On BOOT, the last 3 s before the 10 s wipe pulse
**amber**. When nothing is happening the pixel returns to its network blink: green joined,
cyan not joined, red radio off.

Red is shared by power-off and radio-off because both mean "off"; the durations separate
them. Amber never confirms an action — it stays reserved for the wipe countdown.

### The touch pad (GPIO3)

`TOUCH1` takes an external momentary touch module (pin 1 = `+BATT`, pin 2 = signal,
pin 3 = GND). On the current unit the module works as intended and carries the daily
gestures above. The first pre-release bench board's pad was damaged — it measured a clean
3.3 V while the pin read LOW and latched no edge — because board power button `BT1` puts
`+BATT` (~4.2 V) onto a 3.3 V GPIO through `R12`, and `D6` does not clamp it. That is why
GPIO9 keeps a complete fallback.

Consequences worth knowing before wiring one:

- The firmware assumes a **momentary, active-HIGH** module (`INPUT_PULLUP`/`INPUT_PULLDOWN`
  is set from the `active_high` flag, and the ISR edge follows the same flag). An active-LOW
  or latching module reads as dead or as one endless press, which the firmware eventually
  drops at the 15 s give-up timeout.
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
  and a 3 s hold on either switch brings it back.
- **ArduinoOTA.** `pio run -e C3-OTA -t upload` pushes over the network to `lightpack.local`.
- **Browser OTA.** Upload `firmware.bin` from the page (§1, Option B).
- **USB console.** The full configuration surface over the native USB cable, with no network at
  all. It works in offline mode and even while the board is inside its config hotspot.

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

### USB console — no network needed

The same configuration surface is available over the C3's USB cable, for the bench, for a
board whose antenna cannot hold a link, and for a board parked in offline mode. It is a
**zero-install** static page: Chrome or Edge on a desktop, one URL, no Python and no build.

- **Page:** [`console/`](console/) — open the published path
  `https://machadoleonardo94.github.io/lux-et-tenebris/Firmware/lightpack-C3/console/`, or
  serve the repository root (`python -m http.server 8000`) and open
  `http://localhost:8000/Firmware/lightpack-C3/console/`. If you start the server inside
  `Firmware/lightpack-C3` instead, the same page is at `http://localhost:8000/console/` —
  the path has to match the server's working directory. The page explains its own browser
  requirements; `file://` will not work.
- **Protocol:** [`docs/usb-control.md`](docs/usb-control.md). Line-oriented, `@`-prefixed
  JSON out and `key=value` batches in. A `SET` is transient and `COMMIT` writes flash, so a
  slider drag does not hammer NVS. Presets import/export the same JSON the page uses.
- **While the hotspot is up:** the firmware services USB from the portal loop and keeps
  rendering the strip, so the cable is not dead for the 60 s window. Radio switching is
  refused there; everything else works.
- **One owner:** close `pio device monitor`, the Arduino IDE or any other terminal before
  clicking Connect. The page reports a busy port rather than hanging.
- **Firmware update:** Diagnostics → **Firmware update** checks the `releases/latest` channel and
  writes the app to the inactive OTA slot over the existing USB control link, with chunk and
  percentage progress. Connect first; the running image and NVS are untouched until the final
  `UPDATE END`, so mode, palette, presets, networks and login survive. No ROM bootloader,
  esptool-js or port reset. A board older than `0.5.0-dev` has no `UPDATE` command, so flash
  `0.5.0-dev` once with `pio run -e C3-USB -t upload` or the browser installer; after that the
  console updates it.
- **Connecting resets the board once.** The C3's USB Serial/JTAG resets when the host asserts
  DTR/RTS, so opening the port reboots it; the console waits for the boot and retries the
  handshake. The saved configuration is restored. Unsaved edits do not survive a reconnect.

The USB console never flashes firmware; use the browser installer or PlatformIO for that.

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

> **Windows Device Guard note.** If policy blocks `riscv32-esp-elf-gcc-ar.exe`, the build
> stops with `Error 4551` while archiving the first library. `platformio.ini` therefore runs
> [`tools/build_plain_ar.py`](tools/build_plain_ar.py) as a pre-script, which points the
> archive step at the same toolchain's plain `ar`/`ranlib`. The firmware is built without
> LTO, so the archives are equivalent. Remove the `extra_scripts` line if your policy allows
> the wrapper.

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
| Touch module (`Button_GPIO`) | 3 | `TOUCH1` signal; **daily control**; shared with `BT1` via `R12`, clamped by `D7` (schematic) |
| Battery latch enable | 4 | Driven HIGH at boot; LOW cuts the latch |
| Analog channel 0 | 0 | ADC |
| Free GPIO A / B | 5 / 6 | `PORT3` |
| I²C SDA / SCL | 7 / 8 | 100 kHz, `MPU6050` at 0x68/0x69 |
| BOOT switch | 9 | **Security / recovery / fallback control**; also the ROM's download-mode strap |
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

1. **The touch pad was dead on the first bench board** (§2). The current unit's module works
   and carries the daily gestures; the electrical cause and the repair are documented in
   `docs/pre-release-plan.md`, and GPIO9 remains the fallback and recovery input.
2. **The zener on the schematic is the wrong value.** `D7` was added as `BZV55B2V7` (2.7 V);
   the C3 needs its clamp above VIH (~2.48 V), so 3.3 V or 3.6 V is correct. The `1N4148W` at
   `D6` was never a clamp either — its far end lands on the latch-gate node, not a 3.3 V rail.
3. **The schematic change has no PCB yet.** `lux-et-portabilis-V2.kicad_pcb` and the
   production files were not regenerated for `D7`, so do not order boards expecting it.
4. **No OTA rollback.** The Arduino core never marks an app valid and
   `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is off, so an image that boots but cannot join WiFi
   must be recovered over USB. Keep the last-known-good `firmware.bin`.
5. **The page is unauthenticated by default** (§4).
6. **The USB console is Chromium-only.** Web Serial does not exist in Firefox, Safari or any
   mobile browser, and the page must be served from `https://` or `localhost`. There is no
   native-app fallback in this repository.
7. **`Adafruit SSD1306` / `Adafruit GFX` are in `lib_deps` but no display service exists.**
   `display_started` in `shared/variables.h` is never set by anything.
8. **Announce brightness is unverified behind a case diffuser** — the one open item in
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
│   ├── usb-control.md          USB protocol reference
│   ├── pre-release-plan.md     feature list, menu design, NVS cost analysis, open items
│   └── echoes_buttons_map/     porting notes from the Echoes board (button UX reference)
├── console/                    zero-install Web Serial console (static HTML/CSS/JS)
├── tools/
│   ├── build_plain_ar.py       Windows Device Guard workaround for the archiver
│   └── button_probe/           separate project: measures touch-module polarity
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
    │   ├── usb_control.h       USB line protocol, ticks and events
    │   ├── readings.h          switches, ADC, rail
    │   ├── presets.h           NVS preset slots, versioned format
    │   ├── wifi_settings.h     portal, credentials, offline mode, mDNS
    │   ├── gyros.h             MPU6050, impact detector
    │   ├── i2c_scan.h          bus scan
    │   └── serial_outputs.h    boot report, telemetry
    └── shared/                 pinout, variables, dependencies, library objects
```

Folder names are under `Firmware/lightpack-C3/` except at the top of the list. `install/` at
the **repository root** is the browser flasher, which consumes `releases/*/manifest.json`; it
and `console/` are both static pages that can be published by GitHub Pages.
