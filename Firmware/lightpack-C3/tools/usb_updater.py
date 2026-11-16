#!/usr/bin/env python3
"""Local USB updater for the lightpack-C3.

The board does not need an internet connection. This script runs on the computer,
downloads the published release, verifies its SHA-256, and writes the app over the
same @UPDATE USB protocol the console uses.

Usage:
    python tools/usb_updater.py --dry-run
    python tools/usb_updater.py --port COM9
    python tools/usb_updater.py --base <release-url>/ --port /dev/ttyACM0

Requires pyserial (the PlatformIO Python already has it):
    C:\\Users\\<you>\\.platformio\\penv\\Scripts\\python.exe tools/usb_updater.py

The device is left on the new image only after Update.end() succeeds. NVS, presets,
networks and settings are untouched.
"""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import sys
import time
import urllib.request

DEFAULT_BASE = (
    "https://machadoleonardo94.github.io/lux-et-tenebris/"
    "Firmware/lightpack-C3/releases/latest/"
)
APP_OFFSET = 65536
DEFAULT_CHUNK = 2048
DEFAULT_TIMEOUT = 20.0
BAUD = 115200


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=30) as response:
        return response.read()


def normalize_sha(value: str) -> str:
    return str(value or "").strip().lower()


def load_release(base: str) -> tuple[bytes, str, str]:
    """Return (app image, expected sha256, manifest version)."""
    root = base.rstrip("/")
    manifest = json.loads(fetch(root + "/manifest.json").decode("utf-8"))
    version = str(manifest.get("version", "unknown"))
    parts = (manifest.get("builds") or [{}])[0].get("parts") or []
    app = next(
        (part for part in parts if int(part.get("offset", -1)) == APP_OFFSET),
        None,
    )
    if app is None:
        raise SystemExit("manifest has no app part at 0x10000")
    image = fetch(root + "/" + str(app["path"]))
    expected = normalize_sha(app.get("sha256") or manifest.get("sha256") or "")
    if not expected:
        raise SystemExit("manifest has no sha256 for the app image")
    actual = hashlib.sha256(image).hexdigest()
    if actual != expected:
        raise SystemExit(
            f"checksum mismatch: expected {expected}, downloaded {actual}"
        )
    size = manifest.get("size", len(image))
    if int(size) != len(image):
        raise SystemExit(
            f"size mismatch: manifest {size}, downloaded {len(image)}"
        )
    print(f"release {version}: {len(image)} bytes, sha256 {actual}")
    return image, expected, version


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


def read_ack(ser, timeout: float, command: str, echo_logs: bool) -> dict | None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = ser.readline()
        if not line:
            continue
        text = line.decode(errors="replace").rstrip()
        if text.startswith("@"):
            try:
                frame = json.loads(text[1:])
            except json.JSONDecodeError:
                print(text)
                continue
            if frame.get("cmd") == command or frame.get("t") == command:
                return frame
        elif echo_logs and text:
            print(text)
    return None


def send(ser, line: str) -> None:
    ser.write((line + "\n").encode("ascii"))
    ser.flush()


def open_serial(port: str):
    try:
        import serial
    except ImportError:
        raise SystemExit(
            "pyserial is not installed; use the PlatformIO Python or pip install pyserial"
        )
    ser = serial.Serial(port, BAUD, timeout=0.1)
    ser.dtr = False
    ser.rts = False
    time.sleep(3.0)
    ser.reset_input_buffer()
    return ser


def update(port: str, image: bytes, chunk: int, timeout: float) -> int:
    ser = open_serial(port)
    try:
        #* Opening the CDC port can reset the C3; the boot and the control link
        #? take a few seconds. Retry the ping instead of losing the first one.
        pong = None
        for _ in range(12):
            send(ser, "@PING")
            pong = read_ack(ser, 1.0, "pong", echo_logs=True)
            if pong is not None:
                break
        if pong is None:
            print("no pong from the board (is the firmware running?)", file=sys.stderr)
            return 1
        print(f"board {pong.get('fw')} ({pong.get('build')})")

        send(ser, f"@UPDATE BEGIN {len(image)}")
        ack = read_ack(ser, timeout, "update", echo_logs=True)
        if not ack or not ack.get("ok") or ack.get("state") != "ready":
            print(f"BEGIN refused: {ack}", file=sys.stderr)
            return 1

        offset = 0
        while offset < len(image):
            payload = image[offset : offset + chunk]
            b64 = base64.b64encode(payload).decode("ascii")
            send(ser, f"@UPDATE DATA {offset} {b64}")
            ack = read_ack(ser, timeout, "update", echo_logs=False)
            if not ack:
                print(f"no ACK at {offset}", file=sys.stderr)
                return 1
            if not ack.get("ok"):
                print(f"board rejected chunk: {ack}", file=sys.stderr)
                return 1
            received = ack.get("received")
            if not isinstance(received, int) or received <= offset:
                print(f"chunk not confirmed: {ack}", file=sys.stderr)
                return 1
            offset = min(received, len(image))
            if offset % 32768 < chunk or offset >= len(image):
                print(f"  {offset}/{len(image)} ({offset * 100 // len(image)}%)")

        send(ser, "@UPDATE END")
        ack = read_ack(ser, timeout, "update", echo_logs=True)
        if not ack or not ack.get("ok") or ack.get("state") != "complete":
            print(f"END failed: {ack}", file=sys.stderr)
            return 1
        print("update complete; the board is restarting")
        return 0
    finally:
        ser.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="Local USB updater for lightpack-C3")
    parser.add_argument("--base", default=DEFAULT_BASE, help="release URL ending in /")
    parser.add_argument("--port", default=None, help="serial port (default: auto)")
    parser.add_argument("--chunk", type=int, default=DEFAULT_CHUNK, help="bytes per @UPDATE DATA")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="seconds per ACK")
    parser.add_argument("--dry-run", action="store_true", help="download and verify only")
    args = parser.parse_args()

    image, _sha, _version = load_release(args.base)
    if args.dry_run:
        print("dry run: release verified, no serial port opened")
        return 0

    port = find_port(args.port)
    print(f"port {port}")
    return update(port, image, max(1, args.chunk), max(1.0, args.timeout))


if __name__ == "__main__":
    raise SystemExit(main())
