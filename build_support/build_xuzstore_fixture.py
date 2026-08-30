#!/usr/bin/env python3
"""Build deterministic host inputs for one owned physical xuzstore run."""

from __future__ import annotations

import argparse
from pathlib import Path
import zipfile


def boundary_bytes() -> bytes:
    return bytes((((position * 73) + ((position >> 8) * 19)) ^ 0xA5) & 0xFF
                 for position in range(513))


def zip_info(name: str, directory: bool = False) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    info.compress_type = zipfile.ZIP_STORED
    if directory:
        info.external_attr = (0o40755 << 16) | 0x10
    return info


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--run-id", required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "owner.marker").write_bytes(args.run_id.encode("ascii"))
    hello = b"ReadyOS Ultimate ZIP\r\nStore descriptor probe.\r\n"
    boundary = boundary_bytes()
    (args.output / "HELLO.TXT").write_bytes(hello)
    (args.output / "BOUNDARY.BIN").write_bytes(boundary)
    (args.output / "ZERO.BIN").write_bytes(b"")
    with zipfile.ZipFile(args.output / "HOSTSTORE.ZIP", "w") as archive:
        archive.writestr(zip_info("HOSTEMPTY/", True), b"")
        archive.writestr(zip_info("HOST.TXT"), hello + b"Host-created local sizes.\r\n")
        archive.writestr(zip_info("DEEP/", True), b"")
        archive.writestr(zip_info("DEEP/FILE.BIN"), boundary[:257])
    with zipfile.ZipFile(args.output / "TRAVERSAL.ZIP", "w") as archive:
        archive.writestr(zip_info("../EVIL.TXT"), b"must never extract")
    with zipfile.ZipFile(args.output / "BADCRC.ZIP", "w") as archive:
        archive.writestr(zip_info("BADCRC.TXT"), b"correct bytes")
    bad_crc = bytearray((args.output / "BADCRC.ZIP").read_bytes())
    name_len = int.from_bytes(bad_crc[26:28], "little")
    extra_len = int.from_bytes(bad_crc[28:30], "little")
    bad_crc[30 + name_len + extra_len] ^= 0x80
    (args.output / "BADCRC.ZIP").write_bytes(bad_crc)
    host_store = (args.output / "HOSTSTORE.ZIP").read_bytes()
    (args.output / "TRUNCATED.ZIP").write_bytes(host_store[:-1])
    multi_disk = bytearray(host_store)
    eocd = multi_disk.rfind(b"PK\x05\x06")
    if eocd < 0:
        raise RuntimeError("HOSTSTORE.ZIP has no EOCD")
    multi_disk[eocd + 4] = 1
    (args.output / "MULTIDISK.ZIP").write_bytes(multi_disk)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
