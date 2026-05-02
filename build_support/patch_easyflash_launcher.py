#!/usr/bin/env python3
"""Patch CRT-launcher-only cc65 startup for EasyFlash handoff.

The stock c64 crt0 INIT stub emits a KERNAL BSOUT $0E before initlib. In the
EasyFlash flavor, boot has already performed cartridge banking and the launcher
does its own text-mode setup, so this early KERNAL call is unnecessary and can
be fragile while the cartridge is being quiesced. Patch only launcher_easyflash.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def patch(path: Path) -> None:
    raw = bytearray(path.read_bytes())
    pattern = bytes([0xA9, 0x0E, 0x20, 0xD2, 0xFF, 0x4C])
    offset = raw.find(pattern)
    if offset < 0:
        raise ValueError(f"cc65 c64 init BSOUT pattern not found in {path}")
    if raw.find(pattern, offset + 1) >= 0:
        raise ValueError(f"cc65 c64 init BSOUT pattern is not unique in {path}")
    target_lo = raw[offset + 6]
    target_hi = raw[offset + 7]
    raw[offset:offset + 8] = bytes([0x4C, target_lo, target_hi, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA])
    path.write_bytes(raw)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("prg")
    args = parser.parse_args()
    try:
        patch(Path(args.prg))
    except ValueError as exc:
        print(f"error: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
