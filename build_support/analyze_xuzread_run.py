#!/usr/bin/env python3
"""Validate the physical callback ZIP-reader/catalog result."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import zlib


def u16(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8)


def u32(data: bytes, offset: int) -> int:
    return u16(data, offset) | (u16(data, offset + 2) << 16)


def run_dir_from_log(path: pathlib.Path) -> pathlib.Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return pathlib.Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: pathlib.Path) -> pathlib.Path:
    candidates = [p for p in run_dir.rglob("*xuzread_result_033c*.bin")
                  if p.is_file()]
    if not candidates:
        raise SystemExit(f"no xuzread result dump below {run_dir}")
    return max(candidates, key=lambda p: p.stat().st_mtime)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_log", type=pathlib.Path)
    parser.add_argument("fixtures", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path)
    args = parser.parse_args()

    manifest = json.loads((args.fixtures / "manifest.json").read_text())
    owner = (args.fixtures / "owner.marker").read_bytes()
    run_dir = run_dir_from_log(args.run_log)
    dump = find_dump(run_dir)
    raw = dump.read_bytes()
    at = raw.find(b"XZP1")
    if at < 0 or len(raw) - at < 96:
        raise SystemExit(f"{dump}: missing complete XZP1 result")
    result = raw[at:at + 96]

    if result[4:8] != bytes((1, 1, 7, 0)):
        raise SystemExit(
            f"C64 parser failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} failure=${result[7]:02x} "
            f"reader_error={result[13]} entry={result[14]}"
        )
    if result[8] == 0xFF or result[9] == 0xFF or result[10] == 0xFF or \
            len({result[8], result[9], result[10]}) != 3:
        raise SystemExit("invalid or aliased package/work/catalog banks")
    if result[11:13] != b"\x01\x01":
        raise SystemExit("work/catalog banks were not both released")
    if result[13] != 0 or result[14] != len(manifest["entries"]) or \
            result[15] != len(manifest["entries"]):
        raise SystemExit("reader did not finish the exact central entry count")
    if u16(result, 16) not in (0xDF1C, 0xDE1C, 0xDFFC):
        raise SystemExit("invalid Ultimate UCI base")
    calls = u16(result, 20)
    callback_bytes = u32(result, 22)
    max_request = u16(result, 26)
    if calls < 10 or callback_bytes < manifest["central_size"] or \
            not 1 <= max_request <= 512:
        raise SystemExit("bounded random-access callback evidence is incomplete")
    if u32(result, 28) != manifest["archive_size"] or \
            u32(result, 32) != manifest["central_offset"] or \
            u32(result, 36) != manifest["central_size"]:
        raise SystemExit("C64 archive/central geometry differs from fixture")
    if result[40] != 1 or result[41:44] != bytes((7, 6, 1)):
        raise SystemExit("catalog verification, uZPK v7, or CPU-port restore failed")
    if result[44:48] != zlib.crc32(owner).to_bytes(4, "little"):
        raise SystemExit("fixture cookie mismatch")
    methods = bytes(entry["method"] for entry in manifest["entries"])
    if result[48:55] != methods:
        raise SystemExit("C64 parsed methods differ from fixture")
    directory_mask = sum((1 << i) for i, entry in enumerate(manifest["entries"])
                         if entry["directory"])
    if result[55] != directory_mask:
        raise SystemExit("C64 directory flags differ from fixture")
    if not (result[56] <= 96 and result[57] <= 64 and result[58] <= 160):
        raise SystemExit("fixed diagnostic state windows are too small")

    report = {
        "physical_result": "pass",
        "physical_run_dir": str(run_dir),
        "dump": str(dump),
        "package_bank": result[8],
        "work_bank": result[9],
        "catalog_bank": result[10],
        "banks_released": "pass",
        "uci_base": f"{u16(result, 16):04x}",
        "archive_size": u32(result, 28),
        "central_offset": u32(result, 32),
        "central_size": u32(result, 36),
        "entries": result[14],
        "callback_calls": calls,
        "callback_bytes": callback_bytes,
        "max_callback_request": max_request,
        "catalog_roundtrip": "pass",
        "package_version": result[41],
        "package_phases": result[42],
        "state_sizes": {
            "dos": result[56], "reader": result[57], "record": result[58]
        },
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
