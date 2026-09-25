#!/usr/bin/env python3
"""Verify the native reu_canary.s patterns in a VICE snapshot after OS boot."""
import argparse
import json
from pathlib import Path
import struct


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot", type=Path)
    args = parser.parse_args()
    data = args.snapshot.read_bytes()
    assert data.startswith(b"VICE Snapshot File\x1a\x02\x00")
    assert data[37:50] == b"VICE Version\x1a"
    pos = 58
    reu = None
    while pos < len(data):
        name = data[pos:pos+16].split(b"\0", 1)[0]
        major, minor, size = struct.unpack_from("<BBI", data, pos+16)
        assert size >= 22 and pos+size <= len(data), "Invalid VSF module extent"
        if name == b"REU1764":
            assert (major, minor, size) == (0, 0, 0x100002a)
            # REU v0.0 header/register state precedes its raw 16 MB RAM payload.
            reu = data[pos+42:pos+size]
        pos += size
    assert reu is not None and len(reu) == 0x1000000
    for bank in range(32, 256):
        pattern = bytes(x ^ bank for x in range(256))
        for offset in (0, 0xff00):
            address = bank * 65536 + offset
            assert reu[address:address+256] == pattern, (bank, offset)
    print(json.dumps({"pass": True, "canary_pages": 448, "bytes_checked": 114688,
        "scope": "First and last page of each bank 32..255; not bank interiors",
        "snapshot": str(args.snapshot)}, indent=2))


if __name__ == "__main__":
    main()
