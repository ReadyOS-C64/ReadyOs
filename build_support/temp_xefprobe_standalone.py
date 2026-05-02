#!/usr/bin/env python3
"""
Temporary standalone xefprobe verifier.

Builds confidence in the REU/payload technique without involving cartridge
mapping: the host PRG stashes generated data and an embedded payload into REU,
fetches the payload back to $1000, and jumps into it.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

STOP_SUCCESS = 0xC7F8
STOP_FAIL = 0xC7FA
HOST_FLAG = 0x48
HOST_DATA_FLAG = 0x56
PAYLOAD_FLAG = 0x50
STASH_FLAG = 0x53

MON_CMDS = f"""break {STOP_SUCCESS:04x}
break {STOP_FAIL:04x}
x
m 0400 047f
m c7a0 c7ef
q
"""


def fail(message: str) -> None:
    raise ValueError(message)


def write_monitor(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(MON_CMDS, encoding="ascii")


def parse_monitor_log(path: Path) -> dict[int, bytes]:
    dumps: dict[int, bytes] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"^>C:([0-9a-fA-F]{4})\s+(.*)$", line)
        if not match:
            continue
        addr = int(match.group(1), 16)
        values = re.findall(r"\b([0-9a-fA-F]{2})\b", match.group(2))
        if values:
            dumps[addr] = bytes(int(value, 16) for value in values)
    return dumps


def region_bytes(dumps: dict[int, bytes], start: int, length: int) -> bytes:
    out = bytearray()
    cursor = start
    while len(out) < length:
        chunk = dumps.get(cursor)
        if not chunk:
            break
        out.extend(chunk)
        cursor += len(chunk)
    return bytes(out[:length])


def ascii_to_screen(text: str) -> bytes:
    out = bytearray()
    for ch in text:
        if "A" <= ch <= "Z":
            out.append(ord(ch) - ord("A") + 1)
        elif "0" <= ch <= "9":
            out.append(ord(ch) - ord("0") + 48)
        elif ch == " ":
            out.append(32)
        elif ch == "-":
            out.append(45)
        else:
            out.append(32)
    return bytes(out)


def verify_log(path: Path, screenshot: Path | None, vice_rc: int) -> None:
    text = path.read_text(encoding="utf-8", errors="replace")
    if f"C:${STOP_FAIL:04X}" in text.upper() or f"C:{STOP_FAIL:04X}" in text.upper():
        fail(f"standalone probe stopped in fail trap at ${STOP_FAIL:04X}")
    if f"C:${STOP_SUCCESS:04X}" not in text.upper() and f"C:{STOP_SUCCESS:04X}" not in text.upper():
        fail(f"standalone probe success trap at ${STOP_SUCCESS:04X} was not reached")

    dumps = parse_monitor_log(path)
    screen = region_bytes(dumps, 0x0400, 0x80)
    results = region_bytes(dumps, 0xC7E8, 8)
    ring = region_bytes(dumps, 0xC7A0, 0x50)
    if len(screen) < 16:
        fail("missing screen dump in standalone probe log")
    if len(results) < 4:
        fail("missing result bytes in standalone probe log")

    expected_title = ascii_to_screen("XEFPROBE PAYLOAD")
    if screen[:len(expected_title)] != expected_title:
        fail("payload title mismatch in standalone probe log")

    expected_results = bytes([HOST_FLAG, HOST_DATA_FLAG, PAYLOAD_FLAG, STASH_FLAG])
    if results[:4] != expected_results:
        fail(
            "standalone result flags mismatch: "
            f"expected {expected_results.hex(' ')}, saw {results[:4].hex(' ')}"
        )

    for marker in (b"H", b"D", b"P", b"J", b"S"):
        if marker[0] not in ring:
            fail(f"standalone debug ring missing marker {marker.decode('ascii')}")
    if ord("F") in ring:
        fail("standalone debug ring shows failure marker F")

    if screenshot is not None and not screenshot.exists():
        fail(f"missing standalone screenshot: {screenshot}")

    print("xefprobe standalone verify-log passed")
    print(f"  stop_success: ${STOP_SUCCESS:04X}")
    print(f"  result flags: {results[:4].hex(' ')}")
    if screenshot is not None:
        print(f"  screenshot: {screenshot}")
    if vice_rc != 0:
        print(f"  note: VICE exited with rc={vice_rc}, but the monitor log verified successfully")


def verify_screenshot(path: Path) -> None:
    data = path.read_bytes() if path.exists() else b""
    if not data:
        fail(f"missing standalone screenshot: {path}")
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        fail(f"standalone screenshot is not a PNG: {path}")
    if len(data) < 1024:
        fail(f"standalone screenshot is unexpectedly small: {path}")
    print("xefprobe standalone screenshot verify passed")
    print(f"  screenshot: {path}")


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    write_parser = sub.add_parser("write-monitor")
    write_parser.add_argument("--output", required=True)

    verify_parser = sub.add_parser("verify-log")
    verify_parser.add_argument("--log", required=True)
    verify_parser.add_argument("--screenshot")
    verify_parser.add_argument("--vice-rc", type=int, default=0)

    screenshot_parser = sub.add_parser("verify-screenshot")
    screenshot_parser.add_argument("--screenshot", required=True)

    args = ap.parse_args()

    try:
        if args.cmd == "write-monitor":
            write_monitor(Path(args.output))
            return 0
        if args.cmd == "verify-log":
            verify_log(Path(args.log), Path(args.screenshot) if args.screenshot else None, args.vice_rc)
            return 0
        if args.cmd == "verify-screenshot":
            verify_screenshot(Path(args.screenshot))
            return 0
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
