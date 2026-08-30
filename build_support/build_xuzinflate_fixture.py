#!/usr/bin/env python3
"""Build deterministic raw-DEFLATE inputs for one physical xuzinflate run."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import zlib


class BitWriter:
    def __init__(self) -> None:
        self.bits: list[int] = []

    def little(self, value: int, count: int) -> None:
        self.bits.extend((value >> index) & 1 for index in range(count))

    def huffman(self, code: int, count: int) -> None:
        self.bits.extend((code >> index) & 1
                         for index in range(count - 1, -1, -1))

    def finish(self) -> bytes:
        result = bytearray((len(self.bits) + 7) // 8)
        for index, value in enumerate(self.bits):
            result[index >> 3] |= value << (index & 7)
        return bytes(result)


def fixed_code(symbol: int) -> tuple[int, int]:
    if symbol <= 143:
        return symbol + 0x30, 8
    if symbol <= 255:
        return symbol - 144 + 0x190, 9
    if symbol <= 279:
        return symbol - 256, 7
    return symbol - 280 + 0xC0, 8


def malformed_fixed(kind: str) -> bytes:
    writer = BitWriter()
    writer.little(1, 1)
    writer.little(1, 2)  # final fixed-Huffman block
    if kind == "distance":
        writer.huffman(*fixed_code(257))
        writer.huffman(0, 5)
    elif kind == "length":
        writer.huffman(*fixed_code(286))
    elif kind == "reserved_distance":
        writer.huffman(*fixed_code(257))
        writer.huffman(30, 5)
    else:
        raise ValueError(kind)
    return writer.finish()


def malformed_dynamic(lengths: tuple[int, int, int, int], first_bit: int = 0) -> bytes:
    writer = BitWriter()
    writer.little(1, 1)
    writer.little(2, 2)  # final dynamic-Huffman block
    writer.little(0, 5)  # HLIT = 257
    writer.little(0, 5)  # HDIST = 1
    writer.little(0, 4)  # HCLEN = 4: symbols 16,17,18,0
    for length in lengths:
        writer.little(length, 3)
    writer.little(first_bit, 1)
    return writer.finish()


def require_zlib_rejects(data: bytes, name: str) -> None:
    try:
        zlib.decompress(data, -15)
    except zlib.error:
        return
    raise RuntimeError(f"{name}: host zlib unexpectedly accepted malformed stream")


def raw_deflate(data: bytes, level: int, strategy: int) -> bytes:
    codec = zlib.compressobj(level, zlib.DEFLATED, -15, 8, strategy)
    return codec.compress(data) + codec.flush(zlib.Z_FINISH)


def block_type(packed: bytes) -> int:
    if not packed:
        raise ValueError("empty raw DEFLATE stream")
    return (packed[0] >> 1) & 3


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--run-id", required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "owner.marker").write_bytes(args.run_id.encode("ascii"))

    stored = bytes(((i * 73 + (i >> 3) * 19) ^ 0xA5) & 0xFF
                   for i in range(1025))
    fixed_seed = bytes(((i * 29) ^ (i >> 2) ^ 0x5A) & 0xFF
                       for i in range(1024))
    fixed = (fixed_seed * 40)[:40000]
    dynamic_parts: list[bytes] = []
    for i in range(50000):
        if i % 997 < 780:
            dynamic_parts.append(b"READYOS-ULTIMATE-ZIP/")
        else:
            dynamic_parts.append(bytes(((i * 17 + i // 251) & 0xFF,)))
    dynamic = b"".join(dynamic_parts)[:50000]

    cases = {
        "EMPTY": (b"", 6, zlib.Z_DEFAULT_STRATEGY, None),
        "STORED": (stored, 0, zlib.Z_DEFAULT_STRATEGY, 0),
        "FIXED": (fixed, 6, zlib.Z_FIXED, 1),
        "DYNAMIC": (dynamic, 9, zlib.Z_DEFAULT_STRATEGY, 2),
    }
    manifest: dict[str, object] = {"run_id": args.run_id, "positive": {}}
    for name, (plain, level, strategy, expected_type) in cases.items():
        packed = raw_deflate(plain, level, strategy)
        if expected_type is not None and block_type(packed) != expected_type:
            raise RuntimeError(
                f"{name}: expected first block type {expected_type}, "
                f"got {block_type(packed)}"
            )
        (args.output / f"{name}.RAW").write_bytes(packed)
        (args.output / f"{name}.BIN").write_bytes(plain)
        manifest["positive"][name] = {
            "packed_size": len(packed),
            "size": len(plain),
            "crc32": f"{crc32(plain):08x}",
            "first_block_type": block_type(packed),
        }

    negative_plain = dynamic[:4096]
    base = raw_deflate(negative_plain, 9, zlib.Z_DEFAULT_STRATEGY)
    if block_type(base) != 2:
        raise RuntimeError("negative base must use a dynamic Huffman block")
    (args.output / "TRUNC.RAW").write_bytes(base[:-1])
    (args.output / "TRAIL.RAW").write_bytes(base + b"\xA5")
    (args.output / "BADTYPE.RAW").write_bytes(b"\x07")
    (args.output / "BADSTORED.RAW").write_bytes(
        b"\x01\x01\x00\x00\x00\xA5"
    )
    malformed = {
        "BADDIST": malformed_fixed("distance"),
        "BADLENGTH": malformed_fixed("length"),
        "BADRSVDIST": malformed_fixed("reserved_distance"),
        "BADREPEAT": malformed_dynamic((1, 0, 0, 0)),
        "BADTREE": malformed_dynamic((1, 1, 1, 1)),
    }
    for name, packed in malformed.items():
        require_zlib_rejects(packed, name)
        (args.output / f"{name}.RAW").write_bytes(packed)
    manifest["negative"] = {
        "TRUNC": {"error": 2, "packed_size": len(base) - 1},
        "TRAIL": {"error": 8, "packed_size": len(base) + 1},
        "BADTYPE": {"error": 3, "packed_size": 1},
        "BADSTORED": {"error": 4, "packed_size": 6},
        "BADDIST": {"error": 7, "packed_size": len(malformed["BADDIST"])},
        "BADLENGTH": {"error": 6, "packed_size": len(malformed["BADLENGTH"])},
        "BADRSVDIST": {"error": 7, "packed_size": len(malformed["BADRSVDIST"])},
        "BADREPEAT": {"error": 5, "packed_size": len(malformed["BADREPEAT"])},
        "BADTREE": {"error": 5, "packed_size": len(malformed["BADTREE"])},
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
