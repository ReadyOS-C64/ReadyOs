#!/usr/bin/env python3
"""Validate the physical C64 result and its standards-compatible Store ZIP."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import zipfile


EXPECTED_NAMES = (
    "EMPTY/",
    "HELLO.TXT",
    "NESTED/",
    "NESTED/BOUNDARY.BIN",
    "ZERO.BIN",
)


def run_dir_from_log(path: pathlib.Path) -> pathlib.Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return pathlib.Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: pathlib.Path) -> pathlib.Path:
    candidates = [
        path for path in run_dir.rglob("*xuzstore_results_c000*.bin")
        if path.is_file()
    ]
    if not candidates:
        raise SystemExit(f"no xuzstore C64 result dump below {run_dir}")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def u16(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_log", type=pathlib.Path)
    parser.add_argument("archive", type=pathlib.Path)
    parser.add_argument("fixtures", type=pathlib.Path)
    parser.add_argument("--extracted", type=pathlib.Path)
    parser.add_argument("--extracted-listing", type=pathlib.Path)
    parser.add_argument("--host-extracted", type=pathlib.Path)
    parser.add_argument("--host-extracted-listing", type=pathlib.Path)
    parser.add_argument("--probe-only", action="store_true")
    parser.add_argument("--json-output", type=pathlib.Path)
    args = parser.parse_args()

    run_dir = run_dir_from_log(args.run_log)
    dump_path = find_dump(run_dir)
    dump = dump_path.read_bytes()
    at = dump.find(b"XZS1")
    if at < 0 or len(dump) - at < 128:
        raise SystemExit(f"{dump_path}: missing complete XZS1 result")
    result = dump[at:at + 128]
    if result[4] != 1 or result[5] != 1 or result[6] != 9 or result[7] != 0:
        raise SystemExit(
            f"C64 xuzstore failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} code=${result[7]:02X} entry={result[8]} "
            f"host_entry={result[18]} "
            f"corrupt_case={result[19]} "
            f"zip_error={result[16]} zip_read_error={result[17]} "
            f"reader=(${result[12]:02X},${result[13]:02X}) "
            f"writer=(${result[14]:02X},${result[15]:02X})"
        )
    if result[9] != len(EXPECTED_NAMES) or u16(result, 10) not in (
        0xDF1C, 0xDE1C, 0xDFFC
    ) or result[18] != 4 or result[19] != 4:
        raise SystemExit("invalid entry count or UCI base in C64 result")
    base_report = {
        "physical_result": "pass",
        "entry_count": result[9],
        "archive_size_c64": int.from_bytes(result[20:24], "little"),
        "uci_base": f"{u16(result, 10):04x}",
        "dump": str(dump_path),
        "physical_run_dir": str(run_dir),
        "physical_corrupt_rejections": 4,
    }
    if args.probe_only:
        rendered = json.dumps(base_report, indent=2, sort_keys=True)
        print(rendered)
        if args.json_output:
            args.json_output.write_text(rendered + "\n", encoding="utf-8")
        return 0

    expected = {
        "EMPTY/": b"",
        "HELLO.TXT": (args.fixtures / "HELLO.TXT").read_bytes(),
        "NESTED/": b"",
        "NESTED/BOUNDARY.BIN": (args.fixtures / "BOUNDARY.BIN").read_bytes(),
        "ZERO.BIN": b"",
    }
    raw = args.archive.read_bytes()
    if len(raw) != base_report["archive_size_c64"]:
        raise SystemExit("downloaded archive length differs from C64 writer offset")
    with zipfile.ZipFile(args.archive, "r") as archive:
        infos = archive.infolist()
        names = tuple(info.filename for info in infos)
        if names != EXPECTED_NAMES:
            raise SystemExit(f"central entry order/names mismatch: {names!r}")
        if archive.testzip() is not None:
            raise SystemExit("host ZIP CRC test failed")
        for info in infos:
            if info.compress_type != zipfile.ZIP_STORED or not (info.flag_bits & 0x08):
                raise SystemExit(f"{info.filename}: not streamed method-0 descriptor form")
            if archive.read(info) != expected[info.filename]:
                raise SystemExit(f"{info.filename}: extracted byte mismatch")
    if raw.count(b"PK\x07\x08") != len(EXPECTED_NAMES):
        raise SystemExit("signed data descriptor count mismatch")
    if args.extracted is None:
        raise SystemExit("--extracted is required for the full physical oracle")
    extracted_files = {
        "HELLO.TXT": args.extracted / "HELLO.TXT",
        "NESTED/BOUNDARY.BIN": args.extracted / "NESTED" / "BOUNDARY.BIN",
        "ZERO.BIN": args.extracted / "ZERO.BIN",
    }
    for name, path in extracted_files.items():
        if path.read_bytes() != expected[name]:
            raise SystemExit(f"{name}: C64-extracted byte mismatch")
    if args.extracted_listing is None:
        raise SystemExit("--extracted-listing is required for the full oracle")
    listed = {
        line.strip().rstrip("/")
        for line in args.extracted_listing.read_text(
            encoding="ascii", errors="replace"
        ).splitlines()
        if line.strip()
    }
    if not {"EMPTY", "HELLO.TXT", "NESTED", "ZERO.BIN"}.issubset(listed):
        raise SystemExit(f"C64 extracted root listing is incomplete: {sorted(listed)!r}")
    if args.host_extracted is None or args.host_extracted_listing is None:
        raise SystemExit("host-created ZIP extraction artifacts are required")
    with zipfile.ZipFile(args.fixtures / "HOSTSTORE.ZIP", "r") as host_archive:
        host_infos = host_archive.infolist()
        if any(info.flag_bits & 0x08 for info in host_infos):
            raise SystemExit("host Store fixture unexpectedly uses descriptors")
        host_expected = {
            info.filename: host_archive.read(info)
            for info in host_infos if not info.is_dir()
        }
    host_paths = {
        "HOST.TXT": args.host_extracted / "HOST.TXT",
        "DEEP/FILE.BIN": args.host_extracted / "DEEP" / "FILE.BIN",
    }
    for name, path in host_paths.items():
        if path.read_bytes() != host_expected[name]:
            raise SystemExit(f"{name}: descriptor-free C64 extraction mismatch")
    host_listed = {
        line.strip().rstrip("/")
        for line in args.host_extracted_listing.read_text(
            encoding="ascii", errors="replace"
        ).splitlines()
        if line.strip()
    }
    if not {"HOSTEMPTY", "HOST.TXT", "DEEP"}.issubset(host_listed):
        raise SystemExit(
            f"host ZIP extracted root listing is incomplete: {sorted(host_listed)!r}"
        )

    report = {
        **base_report,
        "archive": str(args.archive),
        "archive_size_host": len(raw),
        "entries": list(EXPECTED_NAMES),
        "host_zip_test": "pass",
        "c64_extracted_files": list(extracted_files),
        "c64_extracted_root_listing": sorted(listed),
        "descriptor_free_host_zip": "pass",
        "host_zip_extracted_files": list(host_paths),
        "host_zip_extracted_root_listing": sorted(host_listed),
        "signed_descriptors": len(EXPECTED_NAMES),
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
