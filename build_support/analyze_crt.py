#!/usr/bin/env python3
"""
Minimal CRT header/chunk analyzer.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


HEADER_LEN = 0x40
CHIP_HEADER_LEN = 0x10


def fail(message: str) -> None:
    raise ValueError(message)


def be16(data: bytes, off: int) -> int:
    return struct.unpack_from(">H", data, off)[0]


def be32(data: bytes, off: int) -> int:
    return struct.unpack_from(">I", data, off)[0]


def parse_crt(path: Path) -> None:
    data = path.read_bytes()
    if len(data) < HEADER_LEN:
        fail("CRT too short")
    if data[:16] != b"C64 CARTRIDGE   ":
        fail("missing CRT signature")

    header_len = be32(data, 0x10)
    version_major = be16(data, 0x14)
    hw_type = be16(data, 0x16)
    exrom = data[0x18]
    game = data[0x19]
    subtype = data[0x1A]
    name = data[0x20:0x40].rstrip(b"\x00").decode("ascii", errors="replace")

    print(f"path: {path}")
    print(f"file_size: {len(data)}")
    print(f"signature: {data[:16].decode('ascii', errors='replace')}")
    print(f"header_len: 0x{header_len:08x}")
    print(f"version_major_minor: {version_major >> 8}.{version_major & 0xff}")
    print(f"hardware_type: {hw_type}")
    print(f"subtype_byte_0x1a: {subtype}")
    print(f"exrom: {exrom}")
    print(f"game: {game}")
    print(f"name: {name}")

    off = header_len
    chip_count = 0
    while off + CHIP_HEADER_LEN <= len(data):
        sig = data[off:off + 4]
        if sig != b"CHIP":
            print(f"chunk_parse_stop: offset 0x{off:06x} signature {sig!r}")
            break
        packet_len = be32(data, off + 4)
        chip_type = be16(data, off + 8)
        bank = be16(data, off + 0x0A)
        load_addr = be16(data, off + 0x0C)
        image_size = be16(data, off + 0x0E)
        print(
            f"chip[{chip_count}]: off=0x{off:06x} type={chip_type} "
            f"bank={bank} load=0x{load_addr:04x} size=0x{image_size:04x} "
            f"packet_len=0x{packet_len:08x}"
        )
        off += packet_len
        chip_count += 1
    print(f"chip_count: {chip_count}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("crt")
    args = ap.parse_args()
    try:
        parse_crt(Path(args.crt))
        return 0
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
