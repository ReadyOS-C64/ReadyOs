#!/usr/bin/env python3
"""Validate the physical recursive create-plan result from C64 Ultimate."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import zlib


def u16(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8)


def run_dir_from_log(path: pathlib.Path) -> pathlib.Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return pathlib.Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: pathlib.Path) -> pathlib.Path:
    candidates = [p for p in run_dir.rglob("*xuzcreateplan_result_033c*.bin")
                  if p.is_file()]
    if not candidates:
        raise SystemExit(f"no xuzcreateplan result dump below {run_dir}")
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
    at = raw.find(b"XZC1")
    if at < 0 or len(raw) - at < 96:
        raise SystemExit(f"{dump}: missing complete XZC1 result")
    result = raw[at:at + 96]

    if result[4:8] != bytes((1, 1, 6, 0)):
        raise SystemExit(
            f"C64 plan failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} failure=${result[7]:02x} "
            f"detail=${result[30]:02x} list_ok={result[31]} "
            f"plan_error={result[11]}"
        )
    if result[8] == 0xFF or result[9] == 0xFF or result[8] == result[9]:
        raise SystemExit("invalid or aliased package/catalog banks")
    if result[10] != 1 or result[11] != 0:
        raise SystemExit("catalog bank was not released or planner reported error")
    expected_counts = (
        manifest["entry_count"], 2, manifest["file_count"],
        manifest["directory_count"], manifest["list_calls"]
    )
    actual_counts = tuple(u16(result, offset)
                          for offset in (12, 14, 16, 18, 20))
    if actual_counts != expected_counts:
        raise SystemExit(
            f"plan counts differ: {actual_counts!r} != {expected_counts!r}"
        )
    if result[22] != 1:
        raise SystemExit("18-entry source directory did not exercise page 1")
    if u16(result, 24) not in (0xDF1C, 0xDE1C, 0xDFFC):
        raise SystemExit("invalid Ultimate UCI base")
    if result[27:30] != bytes((8, 1, 1)):
        raise SystemExit("method, catalog oracle, or owner oracle differs")
    if result[32:36] != zlib.crc32(owner).to_bytes(4, "little"):
        raise SystemExit("fixture cookie mismatch")
    if u16(result, 46) != 149 or not 1800 <= u16(result, 48) <= 1900:
        raise SystemExit("cc65 record/page workspace size contract differs")

    report = {
        "physical_result": "pass",
        "physical_run_dir": str(run_dir),
        "dump": str(dump),
        "package_bank": result[8],
        "catalog_bank": result[9],
        "catalog_bank_released": "pass",
        "uci_base": f"{u16(result, 24):04x}",
        "entries": actual_counts[0],
        "seeds": actual_counts[1],
        "files": actual_counts[2],
        "directories": actual_counts[3],
        "fully_drained_list_calls": actual_counts[4],
        "maximum_page": result[22],
        "catalog_set_oracle": "pass",
        "read_only_output_oracle": "runner-required",
        "state_sizes": {
            "record": u16(result, 46), "browser_page": u16(result, 48)
        },
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
