# Changelog — lightpack-C3

Version history for the Lux et Portabilis V2 firmware.

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
