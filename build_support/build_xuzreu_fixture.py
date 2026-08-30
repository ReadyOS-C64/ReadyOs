#!/usr/bin/env python3
"""Build one deterministic owned-bank physical fixture and C header."""

from __future__ import annotations

import argparse
import re
import zlib
from pathlib import Path

SOURCE_LENGTH = 12000
SOURCE_OFFSET1 = 0x0123
LOAD1_LENGTH = 4096
SHORT_LENGTH = 777
SOURCE_OFFSET2 = SOURCE_LENGTH - SHORT_LENGTH


def source_bytes() -> bytes:
    return bytes(((position * 37) + ((position >> 3) * 11) + 0x5A) & 0xFF
                 for position in range(SOURCE_LENGTH))


def crc_macros(prefix: str, value: bytes) -> str:
    return "".join(f"#define {prefix}_{index} 0x{byte:02X}u\n"
                   for index, byte in enumerate(crc_le(value)))


def crc_le(value: bytes) -> bytes:
    return zlib.crc32(value).to_bytes(4, "little")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--volume", choices=("USB1", "SD"), default="USB1")
    parser.add_argument("--header", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Z0-9][A-Z0-9-]{7,47}", args.run_id):
        raise SystemExit("run id must be 8-48 uppercase letters/digits/hyphens")

    args.output.mkdir(parents=True, exist_ok=True)
    source = source_bytes()
    first = source[SOURCE_OFFSET1:SOURCE_OFFSET1 + LOAD1_LENGTH]
    second = source[SOURCE_OFFSET2:]
    expected = first + second
    cookie = zlib.crc32(args.run_id.encode("ascii")).to_bytes(4, "little")
    (args.output / "owner.marker").write_bytes(args.run_id.encode("ascii"))
    (args.output / "SOURCE.BIN").write_bytes(source)
    (args.output / "EXPECTED.BIN").write_bytes(expected)

    root = f"/{args.volume}/READYOS_UZIP_TEST/{args.run_id}".lower()
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(
        "#ifndef XUZREU_CONFIG_H\n#define XUZREU_CONFIG_H\n\n"
        f'#define XUZREU_OWNED_ROOT "{root}"\n'
        f'#define XUZREU_OWNER_TEXT "{args.run_id.lower()}"\n'
        f"#define XUZREU_OWNER_LENGTH {len(args.run_id)}u\n"
        f'#define XUZREU_SOURCE_PATH "{root}/source"\n'
        f'#define XUZREU_OUTPUT_PATH "{root}/output"\n'
        f"#define XUZREU_SOURCE_LENGTH {SOURCE_LENGTH}u\n"
        f"#define XUZREU_SOURCE_OFFSET1 {SOURCE_OFFSET1}u\n"
        f"#define XUZREU_SOURCE_OFFSET2 {SOURCE_OFFSET2}u\n"
        f"#define XUZREU_OUTPUT_LENGTH {len(expected)}u\n"
        f"#define XUZREU_CONFIG_COOKIE0 0x{cookie[0]:02X}u\n"
        f"#define XUZREU_CONFIG_COOKIE1 0x{cookie[1]:02X}u\n"
        f"#define XUZREU_CONFIG_COOKIE2 0x{cookie[2]:02X}u\n"
        f"#define XUZREU_CONFIG_COOKIE3 0x{cookie[3]:02X}u\n"
        f"{crc_macros('XUZREU_CRC1', first)}"
        f"{crc_macros('XUZREU_CRC2', second)}"
        f"{crc_macros('XUZREU_CRCO', expected)}\n"
        "#endif\n",
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
