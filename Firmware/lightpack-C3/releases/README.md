# Releases — lightpack-C3

Every published build of the Lux et Portabilis V2 firmware, newest first. Each folder is
self-contained: download it, follow its `FLASHING.md`, done. No toolchain required.

**Prefer clicking to typing?** [`install/`](../../../install/) flashes the current release straight
from the browser — Chrome or Edge, a USB-C cable, nothing installed.

| Release | Date | Contents | Notes |
|---|---|---|---|
| [`latest/`](latest/) | dev | App-only `firmware.bin`, `boot_app0.bin`, manifest, hashes | **Development channel**, replaced in place. What the USB console's firmware update button reads. Not a tagged release. |
| [`V0.4-pre/`](V0.4-pre/) | 2026-10-26 | **Compiled binaries** — factory image, app image, bootloader, partition table, OTA selector, `manifest.json` for the browser installer | First pre-release with prebuilt firmware. Custom partition table, browser OTA upload, input rework onto the BOOT switch. |
| [`V0.3/`](V0.3/) | 2026-10-06 | **Source snapshot only** (`lightpack_V03-source.zip`) | No compiled images were published. Build it yourself from the zip. |

## What "pre-release" means here

The firmware is bench-verified but not feature-complete, and there are known hardware issues on
the reference board — chiefly that the `TOUCH1` touch pad input is dead, so all user gestures
live on the ESP32-C3 SuperMini's own BOOT switch. Read
[`../CHANGELOG.md`](../CHANGELOG.md) §Known issues and [`../README.md`](../README.md) §8 before
building one into a prop.

## Licensing of the images

This project's own code is **0BSD** ([`../../../LICENSE`](../../../LICENSE)) — no conditions, no
attribution. The images also link third-party libraries whose terms travel with the binary, so each
release folder carries a `THIRD-PARTY-NOTICES.md` next to its `.bin` files: the Arduino-ESP32 core
(LGPL-2.1-or-later) and Adafruit NeoPixel (LGPL-3.0-or-later) are the ones with real obligations,
everything else is MIT/BSD/Apache. Everything a redistributor has to keep is in
[`../../../THIRD-PARTY-NOTICES.md`](../../../THIRD-PARTY-NOTICES.md).

## For maintainers — cutting a release

1. Build: `pio run -e C3-USB` from `Firmware/lightpack-C3`.
2. Copy `firmware.bin`, `bootloader.bin` and `partitions.bin` out of `.pio/build/C3-USB/`, and
   `boot_app0.bin` from `~/.platformio/packages/framework-arduinoespressif32/tools/partitions/`,
   into a new `releases/V<x.y>/` folder.
3. Merge the factory image (offset order matters):
   ```bash
   pio pkg exec --package="tool-esptoolpy" -- esptool.py --chip esp32c3 merge_bin \
     -o lightpack-C3-V<x.y>-factory.bin --flash_mode qio --flash_freq 80m --flash_size 4MB \
     0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
   ```
4. Add the browser-installer manifest next to the binaries — copy `V0.4-pre/manifest.json`, change
   `version` and the `path`, and keep the part at offset `0` pointing at the **factory** image:
   ```json
   { "name": "lightpack-C3", "version": "V<x.y>",
     "builds": [ { "chipFamily": "ESP32-C3",
                   "parts": [ { "path": "lightpack-C3-V<x.y>-factory.bin", "offset": 0 } ] } ] }
   ```
   Then add a button for it in [`install/`](../../../install/) — see
   [`install/README.md`](../../../install/README.md).
5. Record sizes, SHA-256 hashes and the RAM/flash figures from the build in a `FLASHING.md`,
   add the row above, and add a version section to `../CHANGELOG.md`.
6. Sanity-check the manifest before publishing: `chipFamily` must be exactly `ESP32-C3`, offsets are
   decimal, and `python -m http.server` from the repository root must serve both the page and the
   `.bin` from the same origin (Web Serial fetches, so CORS applies).
7. Tag the commit `lightpack-C3-v<x.y>` and push with `--follow-tags`.

## For maintainers — the `latest/` channel

`latest/` is what the USB console's **Firmware update** button reads. It is not a
release: update it in place after a firmware build that should be installable
from the console.

```bash
pio run -e C3-USB
cp .pio/build/C3-USB/firmware.bin releases/latest/firmware.bin
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin releases/latest/
sha256sum releases/latest/firmware.bin releases/latest/boot_app0.bin > releases/latest/SHA256SUMS.txt
```

Update `latest/manifest.json` (`version`, `built`, `source`, `size`, `sha256`,
and both part `sha256` values) and `latest/BUILD.txt`. The console verifies each
part's hash before it opens the port. The manifest lists **two parts** —
`boot_app0.bin` at `0xE000` and `firmware.bin` at `0x10000` — and deliberately no
factory image: a merged factory image spans the NVS region with `0xFF` padding and
would wipe the saved configuration. `latest/README.md` has the full reasoning.

The console fetches the manifest with `cache: no-store`; no front-end change is
needed for a new build.
