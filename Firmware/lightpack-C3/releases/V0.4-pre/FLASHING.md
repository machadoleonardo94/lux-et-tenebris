# Flashing lightpack-C3 V0.4-pre

Prebuilt firmware for the **Lux et Portabilis V2** board (ESP32-C3 SuperMini, 4 MB flash).
Nothing here needs a compiler, PlatformIO, or a checked-out repository.

**In a hurry?** Use the browser installer — [Route 1](#route-1--browser-installer-no-tools-at-all-recommended-for-a-fresh-board)
— which needs no command line at all. Everything below is the same operation by other means.

| File | Flash offset | What it is |
|---|---:|---|
| `lightpack-C3-V0.4-pre-factory.bin` | `0x0` | **Bootloader + partition table + OTA selector + app**, one write |
| `firmware.bin` | `0x10000` | The application alone — the file the device page and ArduinoOTA use |
| `bootloader.bin` | `0x0` | Second-stage bootloader |
| `partitions.bin` | `0x8000` | Custom partition table (reasoning in `../../partitions_lightpack.csv`) |
| `boot_app0.bin` | `0xE000` | OTA selector; makes the bootloader start `app0` |

`manifest.json` in this folder is what the browser installer reads: it names this release and points
at the factory image at offset `0`. It is not needed for the command-line routes.

## Checksums and sizes

```
lightpack-C3-V0.4-pre-factory.bin   1,334,544 bytes
  5A09F27515B0AF5E246A8E9A882FF508564AFAC4FFB62C98E399897E3E6956FC
firmware.bin                        1,269,008 bytes
  9F4C4240FF47386DD26B070A5A8C93B82272EFAD16F6284DC7043EE35B08947A
bootloader.bin                         13,632 bytes
  5AA3738B392A2C9D2ED12CA52B92939D82BF4D24AF9C5267C85D32C9CCC9C165
partitions.bin                          3,072 bytes
  6D574EF034D3DF9CC43F75E69B0AB9D6A6E6904C8393A1A8CB1A9E789E24ADEF
boot_app0.bin                           8,192 bytes
  F94C5D786A7A8FAB06AC5D10E33BF37711A6697636DC037559EA19CC410A17F0
```

Verify (optional, PowerShell / sha256sum):

```powershell
Get-FileHash lightpack-C3-V0.4-pre-factory.bin -Algorithm SHA256
```
```bash
sha256sum lightpack-C3-V0.4-pre-factory.bin
```

## Route 1 — USB, one command (fresh board, or coming from V0.3)

```bash
esptool.py --chip esp32c3 --port COM6 --baud 460800 write_flash 0x0 lightpack-C3-V0.4-pre-factory.bin
```

- `COM6` → whatever port the SuperMini enumerated as (`/dev/cu.usbmodem*` or `/dev/ttyACM0`
  on macOS/Linux). `esptool.py --chip esp32c3 --port <PORT> chip_id` lists nothing but proves
  the port.
- If the board does not answer, hold **BOOT**, tap **RESET**, release **BOOT** — that is the
  ROM's download mode. (Holding BOOT through a plain reset always enters it; no sketch can
  intercept it.)
- No `esptool.py`? `pip install esptool`, or use the version PlatformIO packages:
  `pio pkg exec --package="tool-esptoolpy" -- esptool.py --chip esp32c3 ...`
- Browsers can also flash this in one shot at <https://espressif.github.io/esptool-js/> —
  connect, pick the port, offset `0x0`, choose the factory image, program.

## Route 1 — browser installer, no tools at all (recommended for a fresh board)

Open [`install/`](../../../../install/) in **Chrome or Edge on a desktop**, plug the board in, click
**Install V0.4-pre**, pick the port. It flashes `lightpack-C3-V0.4-pre-factory.bin` at `0x0` through
the ESP32-C3's native USB, so there is no Python, no esptool and no USB-serial driver to install.

- The page needs `https://` or `localhost`; to run it locally, serve the repository **root** with
  `python -m http.server 8000` and open `http://localhost:8000/install/`.
- It reads this folder's [`manifest.json`](manifest.json), so no offsets are typed by hand.
- Firefox, Safari and mobile browsers have no Web Serial — they get an explanation and a link here.

## Route 2 — USB, one command

```bash
esptool.py --chip esp32c3 --port COM6 --baud 460800 write_flash 0x0 lightpack-C3-V0.4-pre-factory.bin
```

- `COM6` → whatever port the SuperMini enumerated as (`/dev/cu.usbmodem*` or `/dev/ttyACM0`
  on macOS/Linux). `esptool.py --chip esp32c3 --port <PORT> chip_id` lists nothing but proves
  the port.
- If the board does not answer, hold **BOOT**, tap **RESET**, release **BOOT** — that is the
  ROM's download mode. (Holding BOOT through a plain reset always enters it; no sketch can
  intercept it.)
- No `esptool.py`? `pip install esptool`, or use the version PlatformIO packages:
  `pio pkg exec --package="tool-esptoolpy" -- esptool.py --chip esp32c3 ...`
- Espressif's own hosted flasher also works: <https://espressif.github.io/esptool-js/> —
  connect, pick the port, offset `0x0`, choose the factory image, program.

## Route 3 — USB, individual images

Use this when only the application needs replacing and the bootloader and partition table are
already from V0.4-pre or later:

```bash
esptool.py --chip esp32c3 --port COM6 --baud 460800 write_flash \
  0x0     bootloader.bin \
  0x8000  partitions.bin \
  0xe000  boot_app0.bin \
  0x10000 firmware.bin
```

> **Coming from V0.3, do not do this one.** V0.4 moved `app1` from `0x150000` to `0x200000`. A
> board last updated over the air runs from `app1` with the OTA selector pointing there, so the
> selector has to be cleared before the new layout is used:
>
> ```bash
> esptool.py --chip esp32c3 --port COM6 erase_region 0xE000 0x2000
> ```
>
> Route 1 or 2 (or any normal PlatformIO upload, which writes `boot_app0.bin` at `0xE000`) does
> this for you. The full reasoning is in the header of `../../partitions_lightpack.csv`.

## Route 4 — no cable, from the device's own page

For a board already running V0.4-pre or later:

1. Open `http://lightpack.local` (or the board's IP).
2. Open the firmware panel and upload **`firmware.bin`** — not the factory image.
3. The board writes it to the slot it is *not* running, verifies it, replies, and reboots after
   1.5 s.

A failed, truncated or bogus upload leaves the running firmware untouched. **There is no
rollback if the new image boots but misbehaves** (for instance, cannot join your WiFi), so keep
this folder around until the new build is proven.

## Route 5 — ArduinoOTA

From a checkout, with the board on the same network:

```bash
cd Firmware/lightpack-C3
pio run -e C3-OTA -t upload          # uploads to lightpack.local
```

## First boot

1. The status pixel runs the network blink; the strip runs a short self-test (rails and channels
   are printed on serial at **115200 baud**).
2. With no saved network it raises the hotspot **`Lightpack-Devkit`** — join it, and the
   configuration page opens (60 s portal window).
3. Pick your network, then browse to `http://lightpack.local` from anywhere on the LAN.
4. Buttons, with no phone and no browser: **1 click** next mode, **2 clicks** previous mode,
   **5 clicks** power down, **3 s hold** radio on/off, **6 s hold** config hotspot, **10 s hold**
   recovery wipe.

> The page ships unauthenticated and without TLS: anyone who can reach port 80 can change the
> settings and install firmware. Set the page login if the network is not trusted.

## If something goes wrong

| Symptom | Cause / fix |
|---|---|
| Board boots to a blank page, serial shows `I2C FAILED` | Normal if no sensor is fitted; the strip still runs. Check SDA/SCL on `PORT1`. |
| Bootloops after an OTA | The slot selector points at a broken image. Clear it (`erase_region 0xE000 0x2000`) and flash the factory image over USB (Route 1 or 2). |
| No port in the browser's picker | Hold BOOT, tap RESET, release BOOT. Then check the cable carries data and that no serial monitor already owns the port. |
| Board appears as no COM port | Hold BOOT, tap RESET, release BOOT. If still absent, the cable is charge-only or the latch is off — power it from the battery. |
| Page unreachable, no hotspot either | The BOOT switch 3 s hold toggles the radio; if it is off, hold 3 s again. 10 s hold wipes credentials and page login. |
| Mode changes lag by a moment | By design: click gestures resolve 400 ms after release so 1/2/5-click gestures can be told apart. |
