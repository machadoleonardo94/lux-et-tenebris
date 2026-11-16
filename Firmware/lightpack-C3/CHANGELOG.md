# Changelog — lightpack-C3

Version history for the Lux et Portabilis V2 firmware.

---

## [0.6.1-dev] - 2026-11-15

Version stamp and the main-page OTA status.

- `0.6.1-dev` version so an update is distinguishable from the installed build.
- The web upload owns the strip: off during the transfer, purple heartbeat on
  progress, solid on a stall, three red blinks on failure.
- The main page shows the running firmware version.
- Diagnostics read the live averaged rail, so **LED rail** no longer waits for
  the 10 s `[ADC]` telemetry tick.

## [0.6.0-dev] - 2026-11-15

Bench power characterisation, safe mode and the 0.6 configuration schema.

### Power

- Safe-mode recovery after a brown-out: 70% brightness, red/yellow warning, a
  5 s acknowledgement timeout and 1 s of touch blanking after a load test.
- Blocking power stress test and standalone channel-resistance sense: 1-100
  white LEDs, 200 ms / 2 s bursts, test-only rail floor (0 = lab absolute
  limit), 2-6 A ceiling and 500 ms / 2 s steps.
- 0.6 power budget (1000 mA default, 2000 max) and the 10 Hz 3.70 V guard with
  a 3.75 V recovery band; the stored brightness is never rewritten.
- Advisory suggested limit from the last battery test; never applied
  automatically.
- Bench data across 14500, LiPo, USB and no-cell at 2 A, 3.6 A, 4 A and 5 A;
  PMOS envelope 3.6 A continuous / 5 A max / 20 A pulses.

### Schema

- Rainbow flow level (0-255, default 128), applied before the global brightness.
- Gyro-fade tunables: decay, trigger, length gains, colour gain, idle state.
- Preset format v3 (39 fields); v2 slots are reported incompatible. The power
  budget is deliberately device-only and excluded from presets.

### Console

- Power stress and channel-resistance panels with channel mask, current limit,
  fast/precise burst and rail guard.
- Budget, rainbow level and gyro-fade fields; `budget` is accepted by SET and
  reported in the ACK so it autosaves and refreshes.
- Shared summary shows the worst-case load and an explicit budget/guard/safe
  notice.
- Diagnostics fill loop rate, LED rail and free heap from the state when the
  live stream is off.

### Release

- USB OTA owns the strip: it is held off for the whole update. The status pixel
  shows a purple heartbeat while chunks arrive, solid purple on a stall, and
  three red blinks on failure.
- `releases/latest` is now 0.6.0-dev.

## [Unreleased]

Two-button control surface, colour-coded action feedback, and a zero-install USB console.

### Wi-Fi

- **The configuration portal no longer panics when it times out.** WiFiManager's
  own timeout check runs inside `process()`, and it shuts the portal down itself,
  resetting its web server to null. `run_config_portal()` then called
  `stopConfigPortal()` unconditionally on the way out, dereferencing that null
  server (`WiFiManager.cpp:970`) - a Load access fault (`MCAUSE 5`, `MTVAL 0`) and
  a boot loop at exactly the moment the portal was being used to fix credentials.
  The shutdown is now guarded by `getConfigPortalActive()`.
- **A join retry can finally cancel the attempt it is retrying.** The retry used
  `WiFi.disconnect(false, false)`, which returns without touching the driver
  unless the station is already `WL_CONNECTED` - so a station stuck mid-connect
  was never released and every `WiFi.begin()` was refused with
  `ESP_ERR_WIFI_CONN` (0x3007), forever. The join path now calls
  `esp_wifi_disconnect()`, and a 45 s stall watchdog stops and restarts the
  station when an attempt neither joins nor fails.
- **The disconnect reason and the AP's security mode are logged.**
  `[WIFI-EVT] disconnected: reason=...` names the reason in words, and the
  periodic status block reports the AP's `auth` mode, BSSID and the stored
  password length. The framework's automatic reconnect is off: on a refusing AP
  it retried about once a second, hammering the AP and burying the one useful line.

### USB console

- **The whole configuration surface is now available over the USB cable, with no network at
  all.** `src/services/usb_control.h` speaks a line protocol on the same CDC port the console
  uses. It works with the radio switched off and while the board is inside its 60-second
  config hotspot, which is exactly the situation a bad antenna creates.
- **`@PING`, `@GET`, `@SET`, `@COMMIT`, `@RELOAD`, `@STREAM`, `@PRESET`, `@IMPACT`, `@RADIO`
  and `@REBOOT`.** Device frames are `@`-prefixed JSON, host batches are `key=value&…`; every
  other line on the port stays a human log. The full reference is
  [`docs/usb-control.md`](docs/usb-control.md).
- **A `SET` is strict, atomic and transient.** Every key is validated against the 28-field
  table and every value must be numeric before any field is applied; an unknown key rejects
  the whole batch. The ack returns only the fields that moved (including cross-field
  sanitiser changes), so a slider drag does not build the ~3 KB state document or touch NVS.
- **`@COMMIT` is the only USB write to flash.** `@RELOAD` discards RAM edits. A mode change
  made with the physical switches still saves the whole live configuration, as it always did.
- **The HTTP page and the USB console share one writer and one state builder.**
  `webpage_config_apply_arg()` now holds the 28 clamp blocks and both transports call it;
  `webpage_state_json()` is shared the same way. `/api/state` gained `dirty`, `portal` and
  the remaining range constants (`whip_speed_min/max`, `whip_period_min/max`,
  `flame_decay_min`, `flame_spatial_min`, `step_jerk_floor`) so a host does not have to keep
  a second copy of the limits.
- **The console is serviced from the config-portal loop.** `run_config_portal()` calls
  `usb_control_loop()` and `update_strip(mode)` every pass, so the cable answers and the lamp
  reacts while `setup()` is still inside the hotspot. `RADIO` is refused there; tearing the
  radio down underneath WiFiManager is not a supported transition.
- **CDC buffers are sized for batch lines.** The HWCDC RX/TX rings are 4 KB instead of the
  256-byte defaults, and the write timeout drops from 100 ms to 10 ms so a stalled host
  cannot hold the LED loop up for a tenth of a second.
- **New `console/` page: zero-install, Chrome/Edge, Web Serial.** Mode bar, parameter pages,
  live telemetry, presets import/export (the same JSON the device page writes), a device log
  pane and a radio toggle. `?demo` renders it with no board. No build step, no runtime
  dependency.
- **The console can update the firmware over the same USB control link.** Diagnostics →
  Firmware update checks `releases/latest/manifest.json`, verifies the app part's SHA-256, and
  streams it as 2048-byte base64 chunks (`UPDATE BEGIN` / `UPDATE DATA <offset>` / `UPDATE
  END`). The firmware writes the inactive OTA slot through `Update.h` and acknowledges every
  chunk; progress is the board's own received count. The running image and NVS — mode, palette,
  tunables, presets, networks and login — are untouched until `END`. No ROM bootloader,
  esptool-js, port close or reload. The first build with `UPDATE` is `0.5.0-dev`, which has to
  be flashed once with PlatformIO or the installer. Before the first erase it blanks the strip,
  repairs a stale OTA selector so it cannot target the running slot, and keeps the upload state
  across a momentary USB CDC drop while flash is written.
- **Opening the CDC port resets the C3.** The USB Serial/JTAG resets when the host asserts
  DTR/RTS, so the page waits for the boot and retries `PING` for up to 12 s instead of sending
  one request into the reset. The saved configuration is restored; unsaved edits do not
  survive a reconnect. Measured on the bench board with pyserial: DTR high at open resets,
  a later low/low transition resets again, and low/high enters download mode.
- **Read-only protocol detail:** `@STREAM` ticks report the detector's diagnostic counters,
  never `impact_take()`, so telemetry cannot steal an impact from Step flame.

### Firmware

- **GPIO3 (touch pad) is the daily interface again.** It now carries the click ladder:
  1 click next mode, 2 clicks previous mode, 5 clicks power off, 3 s hold radio on/off,
  6 s hold reboots into the WiFi config hotspot. Its old 1 s-hold next-mode and 5 s-hold
  power-off gestures are gone; the click ladder replaces them on both switches.
- **GPIO9 (BOOT) becomes the security / recovery / fallback switch.** 1 click previous
  mode (repeated clicks walk the whole registry), 5 clicks power off, 3 s hold radio
  on/off, 6 s hold config hotspot, 10 s hold recovery wipe. The 10 s wipe stays exclusive
  to GPIO9.
- **Both switches pay the 400 ms click window** because both can now produce a 5-click
  power-off. A mode change therefore resolves 400 ms after the finger lifts.
- **Colour-coded action feedback on both WS2812B channels.** The status pixel (GPIO10)
  shows the action colour; the strip (GPIO1) gives a brief flash of the same colour and
  then the selected mode resumes. The strip flash is current-limited to a peak of 10/255
  per channel (~0.6 A for a 250-pixel white flash).
  - next mode: blue; previous mode: cyan; power off: red (1 s); radio on: green;
    radio off: red; config hotspot: white; recovery wipe: magenta; unrecognised 3/4
    clicks: dim red.
  - while held: green (GPIO3) or purple (GPIO9) below 3 s, amber from 3 s (radio will
    toggle on release), blue from 6 s (hotspot will open). The recovery countdown on
    GPIO9 stays amber.
- **`strip_flash()` is a first-class overlay.** `update_strip()` stands down while a
  feedback flash owns the strip, so the mode cannot repaint over it; the flash itself is
  non-blocking except immediately before a reboot or the latch cut.
- **The page reports the long-press action separately** (`long_action` in `/api/buttons`),
  so a hold no longer claims to be a mode change in the live input panel.
- **The boot report and the live page now describe the two-button map.** The old banner
  still advertised GPIO9 as the whole interface; it now lists GPIO3 daily and GPIO9
  security/recovery separately.
- **The legacy `stmaxg` NVS key is removed only when it exists.** Unconditional
  `preferences.remove()` logged `nvs_erase_key fail: stmaxg NOT_FOUND` on every mode
  change.
- **Power-off says what happened on USB power.** The latch cut is still the shutdown; a
  USB-powered board stays alive afterwards and restarts, so the log now says so instead of
  looking like a failed power-off.

### Tooling

- **The build chain no longer stops on the Device Guard-blocked archiver.** `tools/build_plain_ar.py`
  is loaded as a `pre:` extra script and makes the archive step use the RISC-V toolchain's
  plain `ar`/`ranlib` instead of `riscv32-esp-elf-gcc-ar.exe`. The firmware is built without
  LTO, so the archives are equivalent. This restores the normal PlatformIO
  `pio run -e C3-USB -t upload` task; no manual workaround is needed.

### Verification

`pio run -e C3-USB`:

```
RAM:   12.8% (42,084 / 327,680 bytes)
Flash: 62.5% (1,269,098 / 2,031,616 bytes)
```

Image: `firmware.bin` 1,281,168 bytes. The USB protocol was exercised on the bench board
(`PING`, `GET`, `SET` clamping, strict errors, presets, `STREAM`, `RADIO` off/on, `RELOAD`);
the browser page itself has only been UI-tested with `?demo` so far. The two-button map has
also not been hardware-tested since the rework; the touch module on the current unit is
reported working.

---

## [V0.4-pre] — 2026-10-26

First pre-release of the **renamed** firmware, after the board was identified as the
Portabilis **V2** design and the input hardware on the bench board turned out to be damaged.
Everything below is the work that had accumulated in the working tree since V0.3 — the
directory rename, the partition table, the browser OTA path, the input rework, the presets
format, the diagnostic tool, and the documentation — plus one schematic change.

### Download

Prebuilt images are committed in [`releases/V0.4-pre/`](releases/V0.4-pre/); flashing
instructions and checksums in [`releases/V0.4-pre/FLASHING.md`](releases/V0.4-pre/FLASHING.md).

**Easiest path for a fresh board: [`install/`](../../install/).** A browser flasher — Chrome or Edge,
a USB-C cable, click Install. No Python, no esptool, no USB-serial driver; it fetches this release
through [`manifest.json`](releases/V0.4-pre/manifest.json) and writes the factory image at `0x0`.

| Artifact | Size | SHA-256 |
|---|---:|---|
| `lightpack-C3-V0.4-pre-factory.bin` (flash at `0x0`) | 1,334,544 B | `5A09F27515B0AF5E246A8E9A882FF508564AFAC4FFB62C98E399897E3E6956FC` |
| `firmware.bin` (flash at `0x10000`, or upload to the page) | 1,269,008 B | `9F4C4240FF47386DD26B070A5A8C93B82272EFAD16F6284DC7043EE35B08947A` |
| `bootloader.bin` (`0x0`) | 13,632 B | `5AA3738B392A2C9D2ED12CA52B92939D82BF4D24AF9C5267C85D32C9CCC9C165` |
| `partitions.bin` (`0x8000`) | 3,072 B | `6D574EF034D3DF9CC43F75E69B0AB9D6A6E6904C8393A1A8CB1A9E789E24ADEF` |
| `boot_app0.bin` (`0xE000`) | 8,192 B | `F94C5D786A7A8FAB06AC5D10E33BF37711A6697636DC037559EA19CC410A17F0` |

Built with PlatformIO Core 6.1.19, `espressif32@6.11.0`, `framework-arduinoespressif32` 3.0.1
(Arduino core 3.0.1), esptool 4.5.1, environment `C3-USB`:

```
RAM:   12.5% (40,964 / 327,680 bytes)
Flash: 61.9% (1,256,946 / 2,031,616 bytes)
```

### Firmware

- **Directory renamed `Firmware/Mavuika32-C3` → `Firmware/lightpack-C3`**, so the folder says
  which board it is for. Every path in this file refers to the new name.
- **Custom partition table** (`partitions_lightpack.csv`, selected by `board_build.partitions`).
  The Arduino core's `default.csv` reserved 1.375 MB for a SPIFFS partition nothing ever mounts
  (`LittleFS` is included but never called), leaving the app 68 KB of headroom. The two OTA
  slots now take that space — **1.25 MB → 1.9375 MB each (~770 KB headroom)** — while `nvs`,
  `otadata` and `coredump` keep their exact offsets and sizes. NVS staying put is deliberate:
  a reflash costs a cable, not the configuration, the presets, the saved networks or the login.
  The one moving part is `app1` (`0x150000` → `0x200000`); see *Upgrade notes*.
- **Browser firmware update** (`src/components/ESP32/features/update_firmware_web.h`,
  `GET`/`POST /api/update`). Upload `firmware.bin` from the page — no esptool, no PlatformIO, no
  cable. It writes `esp_ota_get_next_update_partition()`, i.e. the slot the bootloader is *not*
  running; checks the image magic as the first sector arrives and withholds the first 16 bytes
  until the end, so a truncated upload cannot leave a bootable-looking partial image; only
  `Update.end()` marks the new image bootable, and it verifies header, length and checksum
  first. **No rollback** — an image that boots but cannot join WiFi still needs USB.
- **All user input moved to the BOOT switch (GPIO9)**, because the touch pad is hardware-damaged
  on the bench board (§Hardware). 1 click = next mode, 2 clicks = previous mode, 5 clicks = power
  down, 3 s hold = radio on/off, 6 s hold = config hotspot, 10 s hold = recovery wipe.
- **Click classification rewritten.** A press over `BUTTON_CLICK_MAX_MS` (600 ms) is never a
  click, and click counts resolve `BUTTON_CLICK_GAP_MS` (400 ms) after release — separating
  1/2/5-click gestures on release alone is impossible, so the delay is paid deliberately.
  Mode changes therefore happen 400 ms after the finger lifts.
- **Input handling is fully non-blocking.** Classification, actions and every number the page
  draws are polled from `loop()`; the ISR only latches an edge. Previously `check_button()`
  waited for the release in a `delay(10)` loop, which froze `web_server.handleClient()` — the
  page went unanswered for the whole press, a 1 s press stalled strip and HTTP alike, and a held
  switch was invisible to `/api/buttons`.
- **Previous mode** (`led_mode_prev()`) is reachable from the device, not just forward.
- **Mode registry: the `majora` mode (id 3) is retired.** Five modes remain — `solid` (1),
  `flame` (2), `fade` (4), `whip` (5), `rainbow` (6) — and the ids keep their old numbers
  instead of being renumbered, so a value persisted by V0.3 is not silently reinterpreted.
  A stored id with no registry entry is not an error: the configuration load falls back to the
  first entry, so an upgraded board boots lit on `solid` rather than dark.
- **Presets format bumped 1 → 2** (`PRESET_FORMAT_VERSION`), now recording a field offset per
  setting. Version 1 was never bumped when fields were removed, so a 100-byte and a 96-byte
  struct both shipped as version 1 and the version check could not see the difference; a slot
  written by another build is now reported as unreadable instead of drawn as nonsense.
  `lightpack-presets.json` is the exported set of known-good values for the page's import.
- **Web page additions, all new since V0.3** (which served only `/`, `/api/state`,
  `/api/modes`, `/api/mode`, `/api/config`, `/api/presets`, `/api/preset`): network list and
  scan (`/api/wifi`, `/api/wifi/scan`), optional page login (`/api/auth`), live switch state
  (`/api/buttons`) — including the damaged pad, which is how the failure was diagnosed — and
  the firmware update panel (`/api/update`). Every route now passes through `webpage_auth_gate`.
- **Boot diagnostics**: reset-reason report captured as the first statement of `setup()` and
  replayed in periodic telemetry (the C3's USB Serial/JTAG re-enumerates and discards pre-host
  writes, so the boot log alone is unreadable), unloaded rail voltage at boot, minimum rail
  through the strip self-test, and an I²C bus scan that separates "no sensor" from "wrong
  address" from "not an MPU6050".
- **WiFi**: captive portal (SSID `Lightpack-Devkit`, 60 s window), 2 credential slots, offline
  mode that switches the radio off entirely, mDNS name `lightpack.local`.
- **Flash size**: `-fno-exceptions -fno-asynchronous-unwind-tables` added to `build_flags`.
  Order matters and is documented in `platformio.ini`: the platform passes `-fexceptions`, so
  the unwind-table flag alone changes nothing. There is no `throw`/`try`/`catch` in this
  firmware; a throw escaping a prebuilt library now aborts.

### Tooling

- **`install/` — a browser flasher for fresh boards.** ESP Web Tools (pinned to 10.4.0) over the
  Web Serial API, reading `releases/V0.4-pre/manifest.json`. It is the zero-install answer to
  "flash a blank board": no interpreter, no toolchain, and no USB-serial driver, because the C3's
  USB is native CDC. The page diagnoses itself — wrong browser, `file://`, missing secure context,
  component failing to load — and links the command-line route instead of showing a dead button.
  `install/README.md` documents how to run it locally (`python -m http.server` from the repository
  root), how to publish it with Pages, and how to add a channel for the next release.
  Binaries stay in the release folders rather than only on a GitHub Release because Web Serial
  fetches them with `fetch()`, which is same-origin.
- **`tools/button_probe/`** — a separate PlatformIO project (not an environment of the main
  one) that measures the touch module's polarity, so a dead pad can be told apart from a
  wrong-polarity module. It had to be separate: `src_dir` is only honoured in the global
  `[platformio]` section, so the old `[env:C3-PROBE]` flashed the *application* while claiming
  to be a diagnostic.
- `lightpack-presets.json`, `partitions_lightpack.csv`, and `.vscode/` editor settings for the
  new directory.

### Documentation

- `README.md` (new) — user-facing: download, four flashing routes (browser installer first),
  controls, features, web API, build, pinout, partition table, known limitations.
- `docs/pre-release-plan.md` (new) — the feature tiers, the navigation design, the NVS wear
  analysis, the test matrix and the open items. Its status note records that the GPIO3 touch
  pad it was written around is dead on the bench board.
- `install/README.md` — how to run the browser flasher, publish it, and add a release channel.
- This changelog and `releases/` with prebuilt binaries.

### Hardware (same tree)

- `lux-et-portabilis-V2.kicad_sch`: **`D7` added** — `BZV55B2V7`, 2.7 V / 500 mW / 2% zener,
  MiniMELF (`Diode_SMD:D_MiniMELF`), placed on the `Button_GPIO` net with a `GND` return, as the
  clamp for the touch/power-button node.
  **⚠ The value is wrong for this node:** the C3's VIH is ~2.48 V, so a 2.7 V clamp sits too
  close to the logic threshold — 3.3 V or 3.6 V is the part this design wants. Recorded here
  rather than silently corrected in the schematic.
- `lux_et_portabilis/lux_et_portabilis.kicad_prl`: KiCad's `integration_disabled` flag set to
  `false` (editor state, no design change).
- **Schematic only.** `lux-et-portabilis-V2.kicad_pcb` and the production files were **not**
  regenerated for `D7` — do not order boards expecting the clamp.

### Licensing

- **The project is now licensed 0BSD** ([`../../LICENSE`](../../LICENSE)) instead of being
  all-rights-reserved by default. The README described itself as open source while carrying no
  license, which left users with no explicit right to use or pass on the code.
- **[`THIRD-PARTY-NOTICES.md`](../../THIRD-PARTY-NOTICES.md)** audits what the images actually link —
  Arduino-ESP32 core (LGPL-2.1-or-later), Adafruit NeoPixel (LGPL-3.0-or-later), and the MIT/BSD/
  Apache libraries — with per-library copyright lines, the unmodified-source statement and the
  relink instructions the LGPL asks for. Full license texts in [`../../LICENSES/`](../../LICENSES/).
- **Publishing these binaries is what made that necessary:** shipping a `.bin` that statically links
  LGPL code carries notice obligations that a source-only project never met. Each release folder now
  carries a copy of the notices beside the images.

### Upgrade notes

- **V0.3 → V0.4-pre needs one USB flash**, because the partition table changed. A normal
  `pio run -e C3-USB -t upload`, or the factory image at `0x0`, rewrites `boot_app0.bin` and
  leaves `otadata` selecting `app0`, which is what the new table needs.
- **A raw `write_flash` of the app alone does not.** If the board was last updated over the air
  it is running from `app1` with `otadata` pointing there, so clear the selector first:
  `esptool.py --chip esp32c3 --port <PORT> erase_region 0xE000 0x2000`.
- **After the reflash, configuration survives.** Mode, palette, every tunable, all 5 presets,
  the saved networks and the page login are in NVS at its unchanged offset.
- Browser OTA and ArduinoOTA updates from V0.4-pre onward need no cable and no partition step.

### Known issues

Full list with detail in `README.md` §8. The short version:

1. **Touch pad (`GPIO3`) is dead on the bench board** — pad measures 3.3 V, pin reads LOW, no
   edge. `BT1` puts `+BATT` through `R12` onto a 3.3 V GPIO and `D6` does not clamp it. All
   gestures live on `GPIO9`.
2. **`D7` is 2.7 V and should be 3.3–3.6 V** (see Hardware).
3. **`D7` exists in the schematic only** — no PCB, no production files.
4. **No OTA rollback** — keep the last-known-good `firmware.bin`.
5. **The page is unauthenticated by default** and there is no TLS; anyone on port 80 can
   install firmware. Trusted-LAN design, wider-network remote code execution.
6. **`Adafruit SSD1306` / `Adafruit GFX` are unused** — no display service exists.
7. **Announce brightness behind a case diffuser is unverified** (`docs/pre-release-plan.md` §8).

---

## [V0.3] — 2026-10-06

- A **source** snapshot of the firmware, committed at the time as
  `Firmware/Mavuika32-C3/lightpack_V03.zip` and now kept at
  [`releases/V0.3/lightpack_V03-source.zip`](releases/V0.3/lightpack_V03-source.zip).
- No compiled images were published with it; **V0.4-pre is the first release with prebuilt
  binaries.**
