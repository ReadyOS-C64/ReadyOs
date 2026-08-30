#!/usr/bin/env python3
"""Independently validate one-member method-8 ZIPs made by physical uZIP."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import zipfile


CASES = ("EMPTY", "REPEAT", "RANDOM", "CROSS")


def u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "little")


def u32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "little")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def validate_archive(path: Path, member_name: str, expected: bytes) -> dict[str, object]:
    data = path.read_bytes()
    require(len(data) >= 30 + 16 + 46 + 22, f"{path}: archive is too short")
    require(data[:4] == b"PK\x03\x04", f"{path}: local header is missing")

    local_flags = u16(data, 6)
    local_method = u16(data, 8)
    local_name_len = u16(data, 26)
    local_extra_len = u16(data, 28)
    local_name = data[30:30 + local_name_len]
    require(local_flags == 0x0008, f"{path}: local flags are {local_flags:#06x}")
    require(local_method == 8, f"{path}: local method is {local_method}")
    require(u32(data, 14) == 0 and u32(data, 18) == 0 and u32(data, 22) == 0,
            f"{path}: streamed local CRC/sizes are not zero")
    require(local_extra_len == 0, f"{path}: unexpected local extra field")
    require(local_name == member_name.encode("ascii"),
            f"{path}: local member name is {local_name!r}")

    eocd = data.rfind(b"PK\x05\x06")
    require(eocd >= 0 and eocd + 22 == len(data),
            f"{path}: EOCD is missing, commented, or followed by data")
    require(u16(data, eocd + 4) == 0 and u16(data, eocd + 6) == 0,
            f"{path}: multi-disk EOCD is unsupported")
    require(u16(data, eocd + 8) == 1 and u16(data, eocd + 10) == 1,
            f"{path}: expected exactly one central entry")
    require(u16(data, eocd + 20) == 0, f"{path}: unexpected ZIP comment")
    central_size = u32(data, eocd + 12)
    central = u32(data, eocd + 16)
    require(central + central_size == eocd,
            f"{path}: central extent does not end at EOCD")
    require(data[central:central + 4] == b"PK\x01\x02",
            f"{path}: central header is missing")

    central_flags = u16(data, central + 8)
    central_method = u16(data, central + 10)
    crc = u32(data, central + 16)
    compressed_size = u32(data, central + 20)
    plain_size = u32(data, central + 24)
    central_name_len = u16(data, central + 28)
    central_extra_len = u16(data, central + 30)
    central_comment_len = u16(data, central + 32)
    local_offset = u32(data, central + 42)
    central_name = data[central + 46:central + 46 + central_name_len]
    require(central_flags == local_flags, f"{path}: local/central flags differ")
    require(central_method == local_method, f"{path}: local/central methods differ")
    require(central_extra_len == 0 and central_comment_len == 0,
            f"{path}: unexpected central extra field or comment")
    require(local_offset == 0, f"{path}: one-member local offset is not zero")
    require(central_name == local_name, f"{path}: local/central names differ")
    require(central_size == 46 + central_name_len,
            f"{path}: central size does not match its only record")

    payload = 30 + local_name_len + local_extra_len
    descriptor = payload + compressed_size
    require(descriptor + 16 == central,
            f"{path}: payload/descriptor extent does not reach central directory")
    require(data[descriptor:descriptor + 4] == b"PK\x07\x08",
            f"{path}: signed data descriptor is missing")
    require(u32(data, descriptor + 4) == crc,
            f"{path}: descriptor/central CRCs differ")
    require(u32(data, descriptor + 8) == compressed_size,
            f"{path}: descriptor/central compressed sizes differ")
    require(u32(data, descriptor + 12) == plain_size,
            f"{path}: descriptor/central plain sizes differ")
    require(plain_size == len(expected),
            f"{path}: recorded plain size differs from fixture")

    with zipfile.ZipFile(path, "r") as archive:
        entries = archive.infolist()
        require(len(entries) == 1, f"{path}: zipfile found {len(entries)} entries")
        info = entries[0]
        require(info.filename == member_name, f"{path}: zipfile name differs")
        require(info.compress_type == zipfile.ZIP_DEFLATED,
                f"{path}: zipfile method is {info.compress_type}")
        require(info.flag_bits == 0x0008,
                f"{path}: zipfile flags are {info.flag_bits:#06x}")
        actual = archive.read(info)
    require(actual == expected, f"{path}: extracted bytes differ from fixture")

    return {
        "archive_size": len(data),
        "member": member_name,
        "plain_size": plain_size,
        "compressed_size": compressed_size,
        "crc32": f"{crc:08x}",
        "descriptor": "signed",
        "method": 8,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("outputs", type=Path)
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    reports: dict[str, dict[str, object]] = {}
    for case in CASES:
        reports[case] = validate_archive(
            args.outputs / f"{case}.ZIP",
            f"{case}.BIN",
            (args.fixtures / f"{case}.BIN").read_bytes(),
        )
    for case, report in reports.items():
        print(f"{case}: {report}")
    print("physical method-8 ZIP byte oracle: pass")
    if args.json_output:
        args.json_output.write_text(
            json.dumps({"physical_zip8_oracle": "pass", "archives": reports},
                       indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
