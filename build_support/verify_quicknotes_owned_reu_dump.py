#!/usr/bin/env python3
"""Verify QuickNotes-owned allocation types in a full VICE REU dump."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

HEADER_OFF = 0xB600
BANK_TYPES_OFF = 0xB640
BANK_TYPES_SIZE = 0x100
MAGIC = b"RCB5\x05"
REU_APP_ALLOC = 3


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--index", required=True, type=Path)
    parser.add_argument("--expect", required=True, choices=("present", "absent"))
    args = parser.parse_args()

    payload = json.loads(args.index.read_text(encoding="utf-8-sig"))
    readyos_image = None
    for chunk in payload.get("chunks", []):
        path = Path(chunk["file"])
        data = path.read_bytes()
        if len(data) == 0x10000 and data[HEADER_OFF:HEADER_OFF + len(MAGIC)] == MAGIC:
            if readyos_image is not None:
                raise SystemExit("FAIL: more than one full REU bank contains the RCB5 header")
            readyos_image = data
    if readyos_image is None:
        raise SystemExit("FAIL: no full REU bank contains the RCB5 schema header")

    types = readyos_image[BANK_TYPES_OFF:BANK_TYPES_OFF + BANK_TYPES_SIZE]
    owned = [bank for bank, value in enumerate(types) if value == REU_APP_ALLOC]
    if args.expect == "present" and len(owned) < 2:
        raise SystemExit(f"FAIL: expected at least two QuickNotes app-owned banks, found {owned}")
    if args.expect == "absent" and owned:
        raise SystemExit(f"FAIL: app-owned banks survived launcher unload: {owned}")
    print(f"PASS: ReadyOS bank app-owned types are {owned} ({args.expect})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
