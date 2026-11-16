#!/usr/bin/env python3
"""Firmware-agnostic USB installer for the lightpack-C3.

Uses the ESP32-C3 ROM bootloader, so it works even when the installed firmware has
no @UPDATE command (any build older than 0.5.0-dev included).

Put the board in install mode:
    hold GPIO9 (BOOT) while plugging in or pressing RESET.
    Release GPIO9 once the port appears.
The installed firmware is not used; the C3 ROM answers esptool directly.

Then run:
    python tools/usb_installer.py --port COM9

The installer downloads the release, verifies both SHA-256 values, and writes:
    0xE000  boot_app0.bin   selects app0 on the next boot
    0x10000 firmware.bin    the application
The NVS partition at 0x9000 is not touched, so settings, presets, networks and
login survive.

Requires esptool and pyserial. The PlatformIO Python has both:
    C:\\Users\\<you>\\.platformio\\penv\\Scripts\\python.exe tools/usb_installer.py
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path

DEFAULT_BASE = (
    "https://machadoleonardo94.github.io/lux-et-tenebris/"
    "Firmware/lightpack-C3/releases/latest/"
)
BOOT_APP0_OFFSET = 0xE000
APP_OFFSET = 0x10000
DEFAULT_BAUD = 460800


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=30) as response:
        return response.read()


def load_release(base: str) -> tuple[dict, dict, dict]:
    root = base.rstrip("/")
    manifest = json.loads(fetch(root + "/manifest.json").decode("utf-8"))
    parts = (manifest.get("builds") or [{}])[0].get("parts") or []
    by_offset = {int(part.get("offset", -1)): part for part in parts}
    if BOOT_APP0_OFFSET not in by_offset or APP_OFFSET not in by_offset:
        raise SystemExit("manifest is missing the 0xE000 or 0x10000 image")
    images = {}
    for offset, part in by_offset.items():
        image = fetch(root + "/" + str(part["path"]))
        expected = str(part.get("sha256", "")).strip().lower()
        actual = hashlib.sha256(image).hexdigest()
        if expected and actual != expected:
            raise SystemExit(f"checksum mismatch for {part['path']}")
        images[offset] = image
        print(f"{part['path']}: {len(image)} bytes, sha256 {actual}")
    return manifest, images, by_offset


def find_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    try:
        from serial.tools import list_ports
    except ImportError:
        raise SystemExit("pyserial is not installed; pass --port COM9")
    for port in list_ports.comports():
        if port.vid == 0x303A and port.pid == 0x1001:
            return port.device
    raise SystemExit("no lightpack-C3 USB device found; pass --port")


def main() -> int:
    parser = argparse.ArgumentParser(description="Firmware-agnostic lightpack-C3 installer")
    parser.add_argument("--base", default=DEFAULT_BASE, help="release URL ending in /")
    parser.add_argument("--port", default=None, help="serial port (default: auto)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--dry-run", action="store_true", help="download and verify only")
    args = parser.parse_args()

    manifest, images, parts = load_release(args.base)
    version = manifest.get("version", "unknown")
    print(f"release {version}")

    port = find_port(args.port) if not args.dry_run else (args.port or "COM9")
    with tempfile.TemporaryDirectory(prefix="lightpack-install-") as tmp:
        tmp_path = Path(tmp)
        boot_path = tmp_path / "boot_app0.bin"
        app_path = tmp_path / "firmware.bin"
        boot_path.write_bytes(images[BOOT_APP0_OFFSET])
        app_path.write_bytes(images[APP_OFFSET])

        command = [
            sys.executable,
            "-m",
            "esptool",
            "--chip",
            "esp32c3",
            "--port",
            port,
            "--baud",
            str(args.baud),
            "--before",
            "default_reset",
            "--after",
            "hard_reset",
            "write_flash",
            "0xE000",
            str(boot_path),
            "0x10000",
            str(app_path),
        ]
        print("command:", " ".join(command))
        if args.dry_run:
            print("dry run: images verified, esptool not started")
            return 0

        print()
        print("Board not in install mode? Hold GPIO9 (BOOT) while plugging it in or")
        print("pressing RESET, then release GPIO9 once the port appears.")
        print()
        try:
            return subprocess.call(command)
        except FileNotFoundError:
            print(
                "esptool is not available in this Python; use the PlatformIO Python",
                file=sys.stderr,
            )
            return 1


if __name__ == "__main__":
    raise SystemExit(main())
