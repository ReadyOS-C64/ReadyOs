#!/usr/bin/env python3
"""Build deterministic good/conflict/bad-CRC fixtures for xuzextract."""

from __future__ import annotations

import argparse
import io
import json
import pathlib
import random
import re
import struct
import zipfile
import zlib


FIXED_TIME = (2026, 8, 23, 12, 0, 0)


def split32(value: int) -> tuple[int, int]:
    return value & 0xFFFF, (value >> 16) & 0xFFFF


def c_text(value: str) -> str:
    return value.replace("\\", "\\\\").replace('"', '\\"')


def make_info(name: str, method: int, directory: bool = False) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, FIXED_TIME)
    info.compress_type = method
    info.create_system = 3
    info.external_attr = ((0o40755 if directory else 0o100644) << 16)
    if directory:
        info.external_attr |= 0x10
    return info


def local_data(payload: bytes, info: zipfile.ZipInfo) -> bytes:
    at = info.header_offset
    if payload[at:at + 4] != b"PK\x03\x04":
        raise ValueError("bad local signature")
    name_len, extra_len = struct.unpack_from("<HH", payload, at + 26)
    start = at + 30 + name_len + extra_len
    return payload[start:start + info.compress_size]


def central_geometry(payload: bytes) -> tuple[int, int]:
    at = payload.rfind(b"PK\x05\x06")
    if at < 0:
        raise ValueError("EOCD missing")
    size, offset = struct.unpack_from("<II", payload, at + 12)
    return offset, size


def write_good(path: pathlib.Path, store: bytes, deflate: bytes,
               conflict: bytes) -> tuple[bytes, list[zipfile.ZipInfo]]:
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", allowZip64=False) as archive:
        archive.writestr(make_info("nest/", zipfile.ZIP_STORED, True), b"")
        archive.writestr(make_info("nest/deep/", zipfile.ZIP_STORED, True), b"")
        archive.writestr(make_info("nest/store.bin", zipfile.ZIP_STORED), store)
        archive.writestr(make_info("nest/deep/deflate.bin", zipfile.ZIP_DEFLATED),
                         deflate, compresslevel=6)
        archive.writestr(make_info("existing/keep.bin", zipfile.ZIP_STORED),
                         conflict)
        archive.comment = b"XUZEXTRACT-GOOD-V1"
    payload = buffer.getvalue()
    path.write_bytes(payload)
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        infos = archive.infolist()
    if len(infos) != 5 or local_data(payload, infos[3])[0] >> 1 & 3 != 2:
        raise ValueError("fixture did not produce five entries and dynamic Deflate")
    return payload, infos


def write_bad_crc(path: pathlib.Path, content: bytes) -> tuple[bytes, zipfile.ZipInfo,
                                                               int, int]:
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, "w", allowZip64=False) as archive:
        archive.writestr(make_info("bad/crc.bin", zipfile.ZIP_DEFLATED),
                         content, compresslevel=6)
        archive.comment = b"XUZEXTRACT-BADCRC-V1"
    patched = bytearray(buffer.getvalue())
    with zipfile.ZipFile(io.BytesIO(patched)) as archive:
        original = archive.infolist()[0]
    actual_crc = zlib.crc32(content) & 0xFFFFFFFF
    claimed_crc = actual_crc ^ 0x00000001
    struct.pack_into("<I", patched, original.header_offset + 14, claimed_crc)
    central = patched.find(b"PK\x01\x02")
    if central < 0:
        raise ValueError("bad-CRC central record missing")
    struct.pack_into("<I", patched, central + 16, claimed_crc)
    payload = bytes(patched)
    path.write_bytes(payload)
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        info = archive.infolist()[0]
    if info.CRC != claimed_crc or local_data(payload, info)[0] >> 1 & 3 != 2:
        raise ValueError("bad-CRC fixture geometry mismatch")
    return payload, info, actual_crc, claimed_crc


def record_dict(payload: bytes, info: zipfile.ZipInfo) -> dict[str, object]:
    return {
        "name": info.filename,
        "method": info.compress_type,
        "directory": info.is_dir(),
        "size": info.file_size,
        "compressed_size": info.compress_size,
        "local_offset": info.header_offset,
        "crc": info.CRC,
    }


def record_defines(prefix: str, index: int, record: dict[str, object]) -> list[str]:
    size_lo, size_hi = split32(int(record["size"]))
    compressed_lo, compressed_hi = split32(int(record["compressed_size"]))
    offset_lo, offset_hi = split32(int(record["local_offset"]))
    crc = int(record["crc"]).to_bytes(4, "little")
    name = str(record["name"]).encode("ascii")
    return [
        f'#define {prefix}_NAME_{index} "{c_text(str(record["name"]))}"',
        f'#define {prefix}_NAME_BYTES_{index} ' + ", ".join(
            [*(f"0x{value:02X}u" for value in name), "0x00u"]),
        f'#define {prefix}_METHOD_{index} {int(record["method"])}u',
        f'#define {prefix}_DIRECTORY_{index} {1 if record["directory"] else 0}u',
        f'#define {prefix}_SIZE_LO_{index} 0x{size_lo:04X}u',
        f'#define {prefix}_SIZE_HI_{index} 0x{size_hi:04X}u',
        f'#define {prefix}_COMPRESSED_LO_{index} 0x{compressed_lo:04X}u',
        f'#define {prefix}_COMPRESSED_HI_{index} 0x{compressed_hi:04X}u',
        f'#define {prefix}_OFFSET_LO_{index} 0x{offset_lo:04X}u',
        f'#define {prefix}_OFFSET_HI_{index} 0x{offset_hi:04X}u',
        *(f'#define {prefix}_CRC_{index}_{part} 0x{value:02X}u'
          for part, value in enumerate(crc)),
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--header", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if not re.fullmatch(r"XUZEXTRACT-[A-Z0-9-]{8,48}", args.run_id):
        raise SystemExit("run id must be an owned XUZEXTRACT-* identifier")

    args.output.mkdir(parents=True, exist_ok=True)
    args.header.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(0x6502)
    store = bytes(rng.randrange(256) for _ in range(12289))
    phrases = [
        b"READYOS ULTIMATE ZIP STREAMING EXTRACTION ",
        b"REU BANK SNAPSHOT RESTORE CRC COMMIT ",
        b"NESTED PATH PHYSICAL C64 ULTIMATE ",
    ]
    deflate = b"".join(phrases[i % len(phrases)] + bytes((i & 255,))
                       for i in range(900))
    conflict = (b"ARCHIVE MUST NOT REPLACE THIS SENTINEL\r" * 53)[:2049]
    bad_content = b"".join(phrases[(i + 1) % len(phrases)] + bytes((i * 7 & 255,))
                           for i in range(300))
    sentinel = b"C64U EXISTING DESTINATION SENTINEL\r\n"
    owner = args.run_id.lower().encode("ascii")

    good_payload, good_infos = write_good(args.output / "GOOD.ZIP", store,
                                           deflate, conflict)
    bad_payload, bad_info, actual_crc, claimed_crc = write_bad_crc(
        args.output / "BADCRC.ZIP", bad_content)
    (args.output / "owner.marker").write_bytes(owner)
    (args.output / "existing.keep").write_bytes(sentinel)
    (args.output / "expected-store.bin").write_bytes(store)
    (args.output / "expected-deflate.bin").write_bytes(deflate)

    good_records = [record_dict(good_payload, info) for info in good_infos]
    bad_record = record_dict(bad_payload, bad_info)
    good_central, good_central_size = central_geometry(good_payload)
    bad_central, bad_central_size = central_geometry(bad_payload)
    root = f"/usb1/readyos_uzip_test/{args.run_id.lower()}"
    manifest = {
        "run_id": args.run_id,
        "root": root,
        "destination_root": root + "/out",
        "good_archive_size": len(good_payload),
        "good_central_offset": good_central,
        "good_central_size": good_central_size,
        "good_entries": good_records,
        "bad_archive_size": len(bad_payload),
        "bad_central_offset": bad_central,
        "bad_central_size": bad_central_size,
        "bad_entry": bad_record,
        "bad_actual_crc": actual_crc,
        "bad_claimed_crc": claimed_crc,
        "expected_outputs": {
            "NEST/STORE.BIN": len(store),
            "NEST/DEEP/DEFLATE.BIN": len(deflate),
            "EXISTING/KEEP.BIN": len(sentinel),
        },
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    good_lo, good_hi = split32(len(good_payload))
    good_c_lo, good_c_hi = split32(good_central)
    good_cs_lo, good_cs_hi = split32(good_central_size)
    bad_lo, bad_hi = split32(len(bad_payload))
    bad_c_lo, bad_c_hi = split32(bad_central)
    bad_cs_lo, bad_cs_hi = split32(bad_central_size)
    lines = [
        "#ifndef XUZEXTRACT_CONFIG_H",
        "#define XUZEXTRACT_CONFIG_H",
        "",
        f'#define XUZE_OWNED_ROOT "{root}"',
        f'#define XUZE_DEST_ROOT "{root}/out"',
        "#define XUZE_OWNER_BYTES " + ", ".join(
            f"0x{value:02X}u" for value in owner),
        f"#define XUZE_OWNER_LENGTH {len(owner)}u",
        '#define XUZE_GOOD_ARCHIVE "good.zip"',
        '#define XUZE_BAD_ARCHIVE "badcrc.zip"',
        f"#define XUZE_GOOD_COUNT {len(good_records)}u",
        f"#define XUZE_GOOD_ARCHIVE_LO 0x{good_lo:04X}u",
        f"#define XUZE_GOOD_ARCHIVE_HI 0x{good_hi:04X}u",
        f"#define XUZE_GOOD_CENTRAL_LO 0x{good_c_lo:04X}u",
        f"#define XUZE_GOOD_CENTRAL_HI 0x{good_c_hi:04X}u",
        f"#define XUZE_GOOD_CENTRAL_SIZE_LO 0x{good_cs_lo:04X}u",
        f"#define XUZE_GOOD_CENTRAL_SIZE_HI 0x{good_cs_hi:04X}u",
        f"#define XUZE_BAD_ARCHIVE_LO 0x{bad_lo:04X}u",
        f"#define XUZE_BAD_ARCHIVE_HI 0x{bad_hi:04X}u",
        f"#define XUZE_BAD_CENTRAL_LO 0x{bad_c_lo:04X}u",
        f"#define XUZE_BAD_CENTRAL_HI 0x{bad_c_hi:04X}u",
        f"#define XUZE_BAD_CENTRAL_SIZE_LO 0x{bad_cs_lo:04X}u",
        f"#define XUZE_BAD_CENTRAL_SIZE_HI 0x{bad_cs_hi:04X}u",
    ]
    for index, record in enumerate(good_records):
        lines.extend(record_defines("XUZE_GOOD", index, record))
    lines.extend(record_defines("XUZE_BAD", 0, bad_record))
    cookie = zlib.crc32(owner).to_bytes(4, "little")
    lines.extend(f"#define XUZE_COOKIE_{i} 0x{value:02X}u"
                 for i, value in enumerate(cookie))
    lines.extend(("", "#endif", ""))
    args.header.write_text("\n".join(lines), encoding="ascii")
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
