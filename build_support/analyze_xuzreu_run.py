#!/usr/bin/env python3
"""Validate the full-ReadyOS physical owned-bank direct-transfer result."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import zlib


def run_dir_from_log(path: pathlib.Path) -> pathlib.Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return pathlib.Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: pathlib.Path, label: str) -> pathlib.Path:
    candidates = [path for path in run_dir.rglob(f"*{label}*.bin")
                  if path.is_file()]
    if not candidates:
        raise SystemExit(f"no {label} result dump below {run_dir}")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def u16(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8)


def result_at(path: pathlib.Path) -> bytes:
    dump = path.read_bytes()
    at = dump.find(b"XZR1")
    if at < 0 or len(dump) - at < 128:
        raise SystemExit(f"{path}: missing complete XZR1 result")
    return dump[at:at + 128]


def crc_le(value: bytes) -> bytes:
    return zlib.crc32(value).to_bytes(4, "little")


def validate_common(result: bytes, expected: bytes, source: bytes,
                    stage: int) -> dict[str, object]:
    first = source[0x0123:0x0123 + 4096]
    second = source[-777:]
    if result[4] != 1 or result[5] != 1 or result[6] != stage or result[7] != 0:
        raise SystemExit(
            f"C64 xuzreu failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} code=${result[7]:02X} "
            f"reader=(${result[12]:02X},${result[13]:02X}) "
            f"writer=(${result[14]:02X},${result[15]:02X}) "
            f"failure_data={result[58]} failure_status={result[59]}"
        )
    counts = tuple(u16(result, offset) for offset in (16, 18, 20, 22, 24, 26))
    if counts != (4096, 4096, 4096, 777, 4096, 777):
        raise SystemExit(f"unexpected direct transfer counts: {counts!r}")
    if result[28:32] != crc_le(first) or result[32:36] != crc_le(second):
        raise SystemExit("C64 REU range CRC differs from host fixture")
    if result[36:40] != crc_le(expected):
        raise SystemExit("C64 queue-verification CRC differs from expected output")
    if result[40:44] != result[44:48] or result[40:44] == b"\0\0\0\0":
        raise SystemExit("uzpk package CRC changed during the direct transfer")
    if result[49] != 1 or result[50] != 1:
        raise SystemExit("scratch was not freed or package was not app-owned")
    if result[60] != 0:
        raise SystemExit(f"warm ownership check bits were ${result[60]:02X}")
    if result[8] == 0xFF or result[9] == 0xFF or result[8] == result[9]:
        raise SystemExit("invalid package/scratch physical bank IDs")
    if u16(result, 10) not in (0xDF1C, 0xDE1C, 0xDFFC):
        raise SystemExit("invalid UCI base")
    if u16(result, 52) != len(expected):
        raise SystemExit("C64 expected output length mismatch")
    return {
        "stage": stage,
        "package_bank": result[8],
        "scratch_bank": result[9],
        "snapshot_token": result[51],
        "uci_base": f"{u16(result, 10):04x}",
        "load1": counts[1],
        "load2_short": counts[3],
        "save1": counts[4],
        "save2": counts[5],
        "output_length": len(expected),
        "output_crc": result[36:40].hex(),
        "package_crc": result[40:44].hex(),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_log", type=pathlib.Path)
    parser.add_argument("fixtures", type=pathlib.Path)
    parser.add_argument("--downloaded-output", type=pathlib.Path)
    parser.add_argument("--core-only", action="store_true")
    parser.add_argument("--json-output", type=pathlib.Path)
    args = parser.parse_args()

    source = (args.fixtures / "SOURCE.BIN").read_bytes()
    expected = (args.fixtures / "EXPECTED.BIN").read_bytes()
    run_dir = run_dir_from_log(args.run_log)
    core_dump = find_dump(run_dir, "xuzreu_core_c000")
    core = result_at(core_dump)
    core_report = validate_common(core, expected, source, 8)
    report: dict[str, object] = {
        "physical_result": "core-pass" if args.core_only else "pass",
        "physical_run_dir": str(run_dir),
        "core_dump": str(core_dump),
        "core": core_report,
    }
    if not args.core_only:
        resume_dump = find_dump(run_dir, "xuzreu_resume_c000")
        resume = result_at(resume_dump)
        resume_report = validate_common(resume, expected, source, 9)
        if resume[48] != 1 or resume[8] != core[8] or resume[9] != core[9] or \
                resume[40:48] != core[40:48]:
            raise SystemExit("warm resume did not preserve the exact owned-bank state")
        if args.downloaded_output is None:
            raise SystemExit("--downloaded-output is required for the full oracle")
        downloaded = args.downloaded_output.read_bytes()
        if downloaded != expected:
            raise SystemExit("host download differs from expected direct-transfer bytes")
        report.update({
            "resume_dump": str(resume_dump),
            "resume": resume_report,
            "warm_resume": "pass",
            "launcher_unload_reclamation": "pass",
            "host_byte_oracle": "pass",
        })

    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
