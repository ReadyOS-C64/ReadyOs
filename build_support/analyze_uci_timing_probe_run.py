#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys


PHASES = ["open", "info", "read2", "seek", "load", "close"]
LAUNCHER_PHASES = ["first_load", "subsequent_loads"]
LAUNCHER_STAGES = [
    "detect",
    "identify",
    "drvinfo",
    "cd_root",
    "cd_dir",
    "mount",
    "cd_image",
    "open",
    "file_info",
    "read_header",
    "seek_payload",
    "load_reu",
    "close",
]


def find_dump(run_dir: pathlib.Path) -> pathlib.Path:
    candidates = [
        path for path in run_dir.rglob("*uci_timing_results_3000*.bin")
        if path.is_file()
    ]
    candidates += [
        path for path in run_dir.rglob("*3000*.bin")
        if path.is_file() and path not in candidates
    ]
    if not candidates:
        raise SystemExit(f"no uci timing result dump found under {run_dir}")
    return max(candidates, key=lambda p: p.stat().st_mtime)


def u16(data: bytes, off: int) -> int:
    return data[off] | (data[off + 1] << 8)


def decode(path: pathlib.Path) -> dict[str, object]:
    data = path.read_bytes()
    idx = data.find(b"UTIM")
    if idx < 0:
        raise SystemExit(f"{path}: missing UTIM magic")
    b = data[idx:idx + 128]
    version = b[4]
    phase_names = LAUNCHER_PHASES if version >= 4 else (LAUNCHER_PHASES + ["unused2", "unused3", "unused4", "unused5"] if version >= 3 else PHASES)
    raw_phases = {name: u16(b, 16 + i * 2) for i, name in enumerate(phase_names)}
    if version >= 2:
        phases = {name: (ticks * 1000 + 30) // 60 for name, ticks in raw_phases.items()}
        total_ms = (u16(b, 8) * 1000 + 30) // 60
        max_load_ms = (u16(b, 12) * 1000 + 30) // 60
    else:
        phases = raw_phases
        total_ms = u16(b, 8)
        max_load_ms = u16(b, 12)
    if version >= 4:
        max_name = b[48:64].split(b"\0", 1)[0].decode("latin1", errors="replace")
    else:
        max_name = b[32:48].split(b"\0", 1)[0].decode("latin1", errors="replace")
    fail_status = b[51:56].split(b"\0", 1)[0].decode("latin1", errors="replace")
    dir_status = b[62:64].split(b"\0", 1)[0].decode("latin1", errors="replace")
    raw_stage_ticks = None
    stages_ms = None
    raw_subseq_stage_ticks = None
    subseq_stages_ms = None
    if version >= 4:
        count = min(b[15], len(LAUNCHER_STAGES))
        raw_stage_ticks = {
            LAUNCHER_STAGES[i]: u16(b, 20 + i * 2)
            for i in range(count)
        }
        stages_ms = {
            name: (ticks * 1000 + 30) // 60
            for name, ticks in raw_stage_ticks.items()
        }
    if version >= 6:
        count = min(b[15], len(LAUNCHER_STAGES))
        raw_subseq_stage_ticks = {
            LAUNCHER_STAGES[i]: u16(b, 80 + i * 2)
            for i in range(count)
        }
        subseq_stages_ms = {
            name: (ticks * 1000 + 30) // 60
            for name, ticks in raw_subseq_stage_ticks.items()
        }
    if version >= 4:
        first_failure = {
            "item": b[64],
            "launcher_error": b[65],
            "debug_status0": b[66],
            "debug_status1": b[67],
            "transport_trace": b[68:72].hex(),
            "name": b[72:80].split(b"\0", 1)[0].decode("latin1", errors="replace"),
        }
        avg_header_tax_ms = 0
    elif version >= 3:
        first_failure = {
            "item": b[48],
            "launcher_error": b[49],
            "debug_status0": b[50],
            "debug_status1": b[51],
            "name": b[56:64].split(b"\0", 1)[0].decode("latin1", errors="replace"),
        }
        avg_header_tax_ms = 0
    else:
        first_failure = {
            "stage": b[48],
            "status_len": b[49],
            "data_len": b[50],
            "status_prefix": fail_status,
            "data_prefix_hex": b[54:56].hex(),
        }
        avg_header_tax_ms = (phases["read2"] + phases["seek"]) // b[6] if b[6] else 0
    result = {
        "version": version,
        "done": b[5],
        "items": b[6],
        "successes": b[14] if version >= 3 else b[6] - b[7],
        "failures": b[7],
        "total_ms": total_ms,
        "loaded_kb": u16(b, 10),
        "max_load_ms": max_load_ms,
        "phases_ms": phases,
        "stages_ms": stages_ms,
        "raw_stage_ticks": raw_stage_ticks,
        "subsequent_stages_ms": subseq_stages_ms,
        "raw_subsequent_stage_ticks": raw_subseq_stage_ticks,
        "raw_phase_ticks": raw_phases if version >= 2 else None,
        "raw_total_ticks": u16(b, 8) if version >= 2 else None,
        "header_tax_ms": 0 if version >= 3 else phases["read2"] + phases["seek"],
        "avg_header_tax_ms": avg_header_tax_ms,
        "max_load_name": max_name,
        "first_failure": first_failure,
        "dir_probe": {
            "data_len": b[56],
            "status_len": b[57],
            "data_prefix_hex": b[58:62].hex(),
            "status_prefix": dir_status,
        },
        "dump": str(path),
    }
    return result


def manifest_run_dir_from_log(log: pathlib.Path) -> pathlib.Path | None:
    text = log.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', text)
    if not matches:
        return None
    return pathlib.Path(matches[-1]).expanduser().resolve().parent


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("path", help="run directory or log containing Manifest path")
    ap.add_argument("--json-output")
    args = ap.parse_args()

    path = pathlib.Path(args.path).expanduser().resolve()
    if path.is_file():
        run_dir = manifest_run_dir_from_log(path)
        if run_dir is None:
            raise SystemExit(f"{path}: no manifest path found")
    else:
        run_dir = path
    result = decode(find_dump(run_dir))
    print(json.dumps(result, indent=2, sort_keys=True))
    if args.json_output:
        pathlib.Path(args.json_output).write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    return 0 if result["done"] == 1 and result["failures"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
