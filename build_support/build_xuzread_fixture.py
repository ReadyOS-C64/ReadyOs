#!/usr/bin/env python3
"""Build a deterministic descriptor ZIP and cc65 expectations for xuzread."""

from __future__ import annotations

import argparse
import io
import json
import re
import struct
import zipfile
import zlib
from pathlib import Path


class NonSeekable(io.BytesIO):
    def seekable(self) -> bool:
        return False

    def seek(self, *args: object, **kwargs: object) -> int:
        raise io.UnsupportedOperation("non-seekable fixture")


def repeat_bytes() -> bytes:
    return (b"READYOS-UZIP-REPEAT-" * 1700)[:32768]


def random_bytes() -> bytes:
    return bytes(((i * 73) + ((i >> 2) * 29) + 0x31) & 0xFF
                 for i in range(4096))


def cross_bytes() -> bytes:
    return bytes((0x41 + ((i // 2048) & 7)) if (i & 1) == 0
                 else ((i * 19 + 7) & 0xFF) for i in range(16384))


def zip_info(name: str, method: int, directory: bool = False) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, (2026, 8, 23, 12, 0, 0))
    info.compress_type = method
    info.create_system = 3
    info.external_attr = ((0o40775 if directory else 0o100664) << 16)
    if directory:
        info.external_attr |= 0x10
    return info


def c_source_name(name: str) -> str:
    # cc65's C64 target maps lowercase source letters to ASCII uppercase bytes.
    if not re.fullmatch(r"[A-Z0-9_./-]+", name):
        raise ValueError(f"unsupported fixture name {name!r}")
    return name.lower()


def split32(value: int) -> tuple[int, int]:
    return value & 0xFFFF, (value >> 16) & 0xFFFF


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--volume", choices=("USB1", "SD"), default="USB1")
    args = parser.parse_args()
    if not re.fullmatch(r"XUZREAD-[A-Z0-9-]{8,40}", args.run_id):
        raise SystemExit("run id must be an owned XUZREAD-* identifier")

    repeat = repeat_bytes()
    entries = [
        ("ROOT/", b"", zipfile.ZIP_STORED, True),
        ("ROOT/EMPTY.BIN", b"", zipfile.ZIP_DEFLATED, False),
        ("ROOT/REPEAT.BIN", repeat, zipfile.ZIP_DEFLATED, False),
        ("ROOT/SUB/", b"", zipfile.ZIP_STORED, True),
        ("ROOT/SUB/RANDOM.BIN", random_bytes(), zipfile.ZIP_DEFLATED, False),
        ("ROOT/REPEAT.STO", repeat, zipfile.ZIP_STORED, False),
        ("CROSS.BIN", cross_bytes(), zipfile.ZIP_DEFLATED, False),
    ]
    sink = NonSeekable()
    with zipfile.ZipFile(sink, "w", allowZip64=False, compresslevel=6) as archive:
        for name, data, method, directory in entries:
            archive.writestr(zip_info(name, method, directory), data)
        archive.comment = (b"XUZREAD-COMMENT-" +
                           bytes((i * 17 + 3) & 0xFF for i in range(700)))
    payload = sink.getvalue()

    args.output.mkdir(parents=True, exist_ok=True)
    archive_path = args.output / "ARCHIVE.ZIP"
    archive_path.write_bytes(payload)
    (args.output / "owner.marker").write_bytes(args.run_id.encode("ascii"))

    with zipfile.ZipFile(io.BytesIO(payload), "r") as archive:
        infos = archive.infolist()
        if [i.filename for i in infos] != [e[0] for e in entries]:
            raise RuntimeError("fixture entry order changed")

    eocd = payload.rfind(b"PK\x05\x06")
    if eocd < 0:
        raise RuntimeError("fixture EOCD missing")
    central_size, central_offset = struct.unpack_from("<II", payload, eocd + 12)
    cookie = zlib.crc32(args.run_id.encode("ascii")).to_bytes(4, "little")
    root = f"/{args.volume}/READYOS_UZIP_TEST/{args.run_id}".lower()

    lines = [
        "#ifndef XUZREAD_CONFIG_H",
        "#define XUZREAD_CONFIG_H",
        "",
        f'#define XUZREAD_OWNED_ROOT "{root}"',
        f'#define XUZREAD_OWNER_TEXT "{args.run_id.lower()}"',
        f"#define XUZREAD_OWNER_LENGTH {len(args.run_id)}u",
        '#define XUZREAD_ARCHIVE_NAME "archive.zip"',
        f"#define XUZREAD_ENTRY_COUNT {len(infos)}u",
        f"#define XUZREAD_ARCHIVE_LO 0x{split32(len(payload))[0]:04X}u",
        f"#define XUZREAD_ARCHIVE_HI 0x{split32(len(payload))[1]:04X}u",
        f"#define XUZREAD_CENTRAL_LO 0x{split32(central_offset)[0]:04X}u",
        f"#define XUZREAD_CENTRAL_HI 0x{split32(central_offset)[1]:04X}u",
        f"#define XUZREAD_CENTRAL_SIZE_LO 0x{split32(central_size)[0]:04X}u",
        f"#define XUZREAD_CENTRAL_SIZE_HI 0x{split32(central_size)[1]:04X}u",
    ]
    for index, info in enumerate(infos):
        size_lo, size_hi = split32(info.file_size)
        compressed_lo, compressed_hi = split32(info.compress_size)
        offset_lo, offset_hi = split32(info.header_offset)
        crc = info.CRC.to_bytes(4, "little")
        lines.extend([
            f'#define XUZREAD_NAME_{index} "{c_source_name(info.filename)}"',
            f"#define XUZREAD_METHOD_{index} {info.compress_type}u",
            f"#define XUZREAD_DIRECTORY_{index} {1 if info.is_dir() else 0}u",
            f"#define XUZREAD_SIZE_LO_{index} 0x{size_lo:04X}u",
            f"#define XUZREAD_SIZE_HI_{index} 0x{size_hi:04X}u",
            f"#define XUZREAD_COMPRESSED_LO_{index} 0x{compressed_lo:04X}u",
            f"#define XUZREAD_COMPRESSED_HI_{index} 0x{compressed_hi:04X}u",
            f"#define XUZREAD_OFFSET_LO_{index} 0x{offset_lo:04X}u",
            f"#define XUZREAD_OFFSET_HI_{index} 0x{offset_hi:04X}u",
            *(f"#define XUZREAD_CRC_{index}_{part} 0x{byte:02X}u"
              for part, byte in enumerate(crc)),
        ])
    lines.extend(f"#define XUZREAD_COOKIE_{i} 0x{byte:02X}u"
                 for i, byte in enumerate(cookie))
    lines.extend(["", "#endif", ""])
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text("\n".join(lines), encoding="ascii")

    manifest = {
        "archive_size": len(payload),
        "central_offset": central_offset,
        "central_size": central_size,
        "comment_size": len(zipfile.ZipFile(io.BytesIO(payload)).comment),
        "entries": [{
            "name": info.filename,
            "method": info.compress_type,
            "directory": info.is_dir(),
            "size": info.file_size,
            "compressed_size": info.compress_size,
            "crc32": f"{info.CRC:08x}",
            "local_offset": info.header_offset,
            "flags": info.flag_bits,
        } for info in infos],
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
