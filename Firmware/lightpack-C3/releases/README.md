# Releases — lightpack-C3

Every published build of the Lux et Portabilis V2 firmware, newest first. Each folder is
self-contained: download it, follow its `FLASHING.md`, done. No toolchain required.

| Release | Date | Contents | Notes |
|---|---|---|---|
| [`V0.4-pre/`](V0.4-pre/) | 2026-10-26 | **Compiled binaries** — factory image, app image, bootloader, partition table, OTA selector | First pre-release with prebuilt firmware. Custom partition table, browser OTA upload, input rework onto the BOOT switch. |
| [`V0.3/`](V0.3/) | 2026-10-06 | **Source snapshot only** (`lightpack_V03-source.zip`) | No compiled images were published. Build it yourself from the zip. |

## What "pre-release" means here

The firmware is bench-verified but not feature-complete, and there are known hardware issues on
the reference board — chiefly that the `TOUCH1` touch pad input is dead, so all user gestures
live on the ESP32-C3 SuperMini's own BOOT switch. Read
[`../CHANGELOG.md`](../CHANGELOG.md) §Known issues and [`../README.md`](../README.md) §8 before
building one into a prop.

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
4. Record sizes, SHA-256 hashes and the RAM/flash figures from the build in a `FLASHING.md`,
   add the row above, and add a version section to `../CHANGELOG.md`.
5. Tag the commit `lightpack-C3-v<x.y>` and push with `--follow-tags`.
