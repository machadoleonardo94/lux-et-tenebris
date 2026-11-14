# lightpack-C3 — latest build channel

This folder is what the USB console's **Check for latest build** button reads.
It is a development channel, replaced in place as `main` moves; it is **not** a
tagged release. For a blank board or a full recovery, use the browser installer
or a versioned folder under `releases/`.

## What is in here

| File | Purpose |
|---|---|
| `manifest.json` | Update manifest (ESP Web Tools-compatible) plus the version/date/size/SHA fields the console displays |
| `firmware.bin` | App-only image, written at `0x10000` |
| `boot_app0.bin` | OTA selector, written at `0xE000`; makes the bootloader start app0 |
| `SHA256SUMS.txt` | Hashes of both images |
| `BUILD.txt` | Source commit and build time |

The console writes only the app slot and the OTA selector, with
`eraseAll: false`. The NVS partition (`0x9000`–`0xE000`) is not touched, so mode,
palette, every tunable, the five preset slots, the saved networks and the page
login all survive. The board is forced to boot `app0`; the next web/OTA update
will use `app1` as usual.

Each manifest part carries its own `sha256`; the console downloads both, verifies
them with Web Crypto, and only then opens the port. A mismatch stops the update
before anything is written.

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
`manifest.json` with `cache: no-store` and the images with a source-version
query, so a refresh sees the new build without a code change.

## Why app-only and not the factory image

The factory image spans `0x0`–end and contains `0xFF` padding across the NVS
region when merged with `esptool merge_bin`. Writing it at `0x0` would wipe the
saved configuration. This channel writes exactly two parts — the OTA selector
and the app — so an update is an update, not a factory reset.
