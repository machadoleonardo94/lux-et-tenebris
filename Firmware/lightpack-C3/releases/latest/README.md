# lightpack-C3 — latest build channel

This folder is what the USB console's **Check for latest build** button reads.
It is a development channel, replaced in place as `main` moves; it is **not** a
tagged release. For a blank board or a full recovery, use the browser installer
or a versioned folder under `releases/`.

## What is in here

| File | Purpose |
|---|---|
| `manifest.json` | Update manifest (ESP Web Tools-compatible) plus the version/date/size/SHA fields the console displays |
| `firmware.bin` | App-only image; the console sends it to the inactive OTA slot |
| `boot_app0.bin` | OTA selector at `0xE000` for ESP Web Tools-compatible manifests; the console does **not** send it |
| `SHA256SUMS.txt` | Hashes of both images |
| `BUILD.txt` | Version, build time and source channel |

The console picks the part at offset `0x10000`, verifies its SHA-256 with Web
Crypto, and streams it over the existing `@` link (`UPDATE BEGIN/DATA/END`).
The firmware writes it through `Update.h` to the inactive OTA slot. The running
image and the NVS partition (`0x9000`–`0xE000`) are untouched until `UPDATE END`
marks the new slot bootable, so mode, palette, every tunable, the five preset
slots, the saved networks and the page login all survive.

`boot_app0.bin` is kept because an ESP Web Tools manifest can carry it, but the
console's USB path never writes it. The board chooses the boot slot through the
OTA selector that `Update.end()` updates.

## First install of this protocol

`UPDATE` exists from **0.5.0-dev**. A board running an older build has no such
command; flash `0.5.0-dev` once with `pio run -e C3-USB -t upload` or the browser
installer. After that the console can update it over USB without the ROM
bootloader.

## Updating it

After a firmware build that should become the latest channel:

```bash
pio run -e C3-USB
cp .pio/build/C3-USB/firmware.bin releases/latest/firmware.bin
cp ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin releases/latest/
sha256sum releases/latest/firmware.bin releases/latest/boot_app0.bin > releases/latest/SHA256SUMS.txt
```

Then update `manifest.json` (`version`, `built`, `source`, `size`, `sha256`, and
both part `sha256` values) and `BUILD.txt`, and commit both. The console fetches
`manifest.json` with `cache: no-store` and the image with a source-version query,
so a refresh sees the new build without a code change.

## Why app-only

The factory image spans `0x0`–end and contains `0xFF` padding across the NVS
region when merged with `esptool merge_bin`. Writing it at `0x0` would wipe the
saved configuration. This channel carries the app only; the console's USB path
writes it to the inactive OTA slot and lets the bootloader switch slots.
