#!/usr/bin/env python3
"""Strict oracle for the physical seven-entry mixed uZIP archive."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import zipfile
import zlib


def u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "little")


def u32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "little")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def expected_entries(fixtures: Path) -> list[tuple[str, int, bool, bytes]]:
    return [
        ("ROOT/", 0, True, b""),
        ("ROOT/EMPTY.BIN", 8, False, (fixtures / "EMPTY.BIN").read_bytes()),
        ("ROOT/REPEAT.BIN", 8, False, (fixtures / "REPEAT.BIN").read_bytes()),
        ("ROOT/SUB/", 0, True, b""),
        ("ROOT/SUB/RANDOM.BIN", 8, False,
         (fixtures / "RANDOM.BIN").read_bytes()),
        ("ROOT/REPEAT.STO", 0, False, (fixtures / "REPEAT.BIN").read_bytes()),
        ("CROSS.BIN", 8, False, (fixtures / "CROSS.BIN").read_bytes()),
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    expected = expected_entries(args.fixtures)
    data = args.archive.read_bytes()
    require(len(data) >= 22, "MULTI.ZIP is too short")
    eocd = data.rfind(b"PK\x05\x06")
    require(eocd >= 0 and eocd + 22 == len(data),
            "EOCD is missing, commented, or followed by data")
    require(u16(data, eocd + 4) == 0 and u16(data, eocd + 6) == 0,
            "multi-disk EOCD is unsupported")
    require(u16(data, eocd + 8) == len(expected) and
            u16(data, eocd + 10) == len(expected),
            "EOCD does not advertise exactly seven entries")
    require(u16(data, eocd + 20) == 0, "unexpected ZIP comment")
    central_size = u32(data, eocd + 12)
    central_offset = u32(data, eocd + 16)
    require(central_offset + central_size == eocd,
            "central directory extent does not end at EOCD")

    central_at = central_offset
    records: list[dict[str, int | str | bool]] = []
    for index, (name, method, directory, plain) in enumerate(expected):
        require(data[central_at:central_at + 4] == b"PK\x01\x02",
                f"entry {index}: central header is missing")
        flags = u16(data, central_at + 8)
        actual_method = u16(data, central_at + 10)
        crc = u32(data, central_at + 16)
        compressed_size = u32(data, central_at + 20)
        plain_size = u32(data, central_at + 24)
        name_len = u16(data, central_at + 28)
        extra_len = u16(data, central_at + 30)
        comment_len = u16(data, central_at + 32)
        disk_start = u16(data, central_at + 34)
        external = u32(data, central_at + 38)
        local_offset = u32(data, central_at + 42)
        encoded_name = data[central_at + 46:central_at + 46 + name_len]
        require(encoded_name == name.encode("ascii"),
                f"entry {index}: central name {encoded_name!r} != {name!r}")
        require(flags == 0x0008, f"{name}: flags are {flags:#06x}")
        require(actual_method == method,
                f"{name}: method is {actual_method}, expected {method}")
        require(extra_len == 0 and comment_len == 0 and disk_start == 0,
                f"{name}: central extra/comment/disk field is nonzero")
        require(bool(external & 0x10) == directory,
                f"{name}: DOS directory attribute differs")
        require(plain_size == len(plain), f"{name}: plain size differs")
        require(crc == (zlib.crc32(plain) & 0xFFFFFFFF),
                f"{name}: CRC differs from fixture")
        if method == 0:
            require(compressed_size == plain_size,
                    f"{name}: Store compressed size differs")
        records.append({
            "name": name,
            "method": method,
            "directory": directory,
            "crc": crc,
            "compressed_size": compressed_size,
            "plain_size": plain_size,
            "local_offset": local_offset,
        })
        central_at += 46 + name_len
    require(central_at == eocd, "central records do not consume central extent")

    report_entries: list[dict[str, object]] = []
    for index, ((name, method, directory, plain), record) in enumerate(
            zip(expected, records)):
        local = int(record["local_offset"])
        require(data[local:local + 4] == b"PK\x03\x04",
                f"{name}: local header is missing")
        require(u16(data, local + 6) == 0x0008,
                f"{name}: local flags differ")
        require(u16(data, local + 8) == method,
                f"{name}: local method differs")
        require(u32(data, local + 14) == 0 and
                u32(data, local + 18) == 0 and
                u32(data, local + 22) == 0,
                f"{name}: streamed local CRC/sizes are nonzero")
        name_len = u16(data, local + 26)
        extra_len = u16(data, local + 28)
        local_name = data[local + 30:local + 30 + name_len]
        require(extra_len == 0 and local_name == name.encode("ascii"),
                f"{name}: local name or extra field differs")
        payload_at = local + 30 + name_len
        compressed_size = int(record["compressed_size"])
        payload = data[payload_at:payload_at + compressed_size]
        descriptor = payload_at + compressed_size
        require(data[descriptor:descriptor + 4] == b"PK\x07\x08",
                f"{name}: signed descriptor is missing")
        require(u32(data, descriptor + 4) == record["crc"] and
                u32(data, descriptor + 8) == compressed_size and
                u32(data, descriptor + 12) == record["plain_size"],
                f"{name}: descriptor differs from central record")
        next_offset = (int(records[index + 1]["local_offset"])
                       if index + 1 < len(records) else central_offset)
        require(descriptor + 16 == next_offset,
                f"{name}: local entry extent is not contiguous")
        if method == 0:
            decoded = payload
        else:
            decoder = zlib.decompressobj(-15)
            decoded = decoder.decompress(payload) + decoder.flush()
            require(decoder.eof and not decoder.unused_data and
                    not decoder.unconsumed_tail,
                    f"{name}: raw Deflate stream is not exact")
        require(decoded == plain, f"{name}: decoded bytes differ from fixture")
        report_entries.append({
            "name": name,
            "method": method,
            "directory": directory,
            "plain_size": len(plain),
            "compressed_size": compressed_size,
            "crc32": f"{int(record['crc']):08x}",
            "local_offset": local,
        })

    with zipfile.ZipFile(args.archive, "r") as archive:
        infos = archive.infolist()
        require([info.filename for info in infos] == [item[0] for item in expected],
                "zipfile entry order/names differ")
        for info, (name, method, directory, plain) in zip(infos, expected):
            require(info.compress_type == method,
                    f"{name}: zipfile method differs")
            require(info.flag_bits == 0x0008,
                    f"{name}: zipfile flags differ")
            require(info.is_dir() == directory,
                    f"{name}: zipfile directory classification differs")
            require(archive.read(info) == plain,
                    f"{name}: zipfile extraction differs")

    report = {
        "physical_multi_zip_oracle": "pass",
        "archive_size": len(data),
        "central_offset": central_offset,
        "central_size": central_size,
        "entries": report_entries,
        "python_zipfile_extraction": "pass",
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
