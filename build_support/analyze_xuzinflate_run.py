#!/usr/bin/env python3
"""Validate physical xuzinflate result and independent decoded byte oracles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import zlib


POSITIVE = ("EMPTY", "STORED", "FIXED", "DYNAMIC")


def run_dir_from_log(path: Path) -> Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: Path) -> Path:
    candidates = [
        path for path in run_dir.rglob("*xuzinflate_results_033c*.bin")
        if path.is_file()
    ]
    if not candidates:
        raise SystemExit(f"no xuzinflate result dump below {run_dir}")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def jiffy(data: bytes, offset: int) -> int:
    return (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_log", type=Path,
                        help="runner log or detached physical artifact directory")
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("--outputs", type=Path)
    parser.add_argument("--probe-only", action="store_true")
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    run_dir = (args.run_log.expanduser().resolve() if args.run_log.is_dir()
               else run_dir_from_log(args.run_log))
    dump_path = find_dump(run_dir)
    dump = dump_path.read_bytes()
    at = dump.find(b"XZI1")
    if at < 0 or len(dump) - at < 128:
        raise SystemExit(f"{dump_path}: missing complete XZI1 result")
    result = dump[at:at + 128]
    if (result[4], result[5], result[6], result[7]) != (1, 1, 6, 0):
        raise SystemExit(
            f"C64 xuzinflate failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} code=${result[7]:02X} positive={result[8]} "
            f"negative={result[9]} inflate_error={result[16]} "
            f"input=(${result[12]:02X},${result[13]:02X}) "
            f"output=(${result[14]:02X},${result[15]:02X})"
        )
    if result[8] != 4 or result[9] != 11 or result[17] != 4 or result[18] != 11:
        raise SystemExit("C64 did not complete all positive and negative cases")
    uci = result[10] | (result[11] << 8)
    if uci not in (0xDF1C, 0xDE1C, 0xDFFC):
        raise SystemExit(f"unexpected UCI base ${uci:04X}")
    if result[12] or result[14]:
        raise SystemExit("C64 result contains UCI transport flags")
    progress_size = int.from_bytes(result[72:76], "little")
    progress_dictionary = int.from_bytes(result[76:78], "little")
    if (progress_size, progress_dictionary) != (50000, 50000 - 32768):
        raise SystemExit(
            "final dynamic progress did not prove the 32K ring wrap: "
            f"size={progress_size} dictionary={progress_dictionary}"
        )
    if result[78:80] != b"\xA5\x5A":
        raise SystemExit(
            f"dictionary guard changed: {result[78:80].hex()}"
        )
    stack_initial = int.from_bytes(result[80:82], "little")
    stack_low = int.from_bytes(result[82:84], "little")
    if not (0xC400 <= stack_low <= stack_initial <= 0xD000):
        raise SystemExit(
            "cc65 software stack escaped its reserved physical-probe window: "
            f"initial=${stack_initial:04X} low=${stack_low:04X}"
        )
    stack_bytes = 0xD000 - stack_low
    if stack_bytes > 512:
        raise SystemExit(
            "cc65 inflater path exceeds the ReadyOS 512-byte stack budget: "
            f"{stack_bytes} bytes (low=${stack_low:04X})"
        )

    timing: dict[str, object] = {}
    for index, name in enumerate(POSITIVE):
        start = jiffy(result, 24 + index * 3)
        end = jiffy(result, 40 + index * 3)
        ticks = (end - start) & 0xFFFFFF
        timing[name] = {"jiffies": ticks, "seconds_at_60hz": round(ticks / 60, 3)}

    report: dict[str, object] = {
        "physical_result": "pass",
        "physical_run_dir": str(run_dir),
        "dump": str(dump_path),
        "uci_base": f"{uci:04x}",
        "positive_cases": list(POSITIVE),
        "negative_rejections": [
            "TRUNC", "TRAIL", "BADTYPE", "BADSTORED", "BADDIST",
            "BADLENGTH", "BADRSVDIST", "BADREPEAT", "BADTREE",
            "OUTPUT_TOO_SMALL", "OUTPUT_TOO_LARGE",
        ],
        "dictionary_ring": {
            "final_output_size": progress_size,
            "final_position": progress_dictionary,
            "guards": result[78:80].hex(),
        },
        "cc65_software_stack": {
            "initial": f"{stack_initial:04x}",
            "low_water": f"{stack_low:04x}",
            "bytes_below_d000": stack_bytes,
            "reserved_floor": "c400",
        },
        "timing": timing,
    }
    if not args.probe_only:
        if args.outputs is None:
            raise SystemExit("--outputs is required for the complete oracle")
        manifest = json.loads((args.fixtures / "manifest.json").read_text())
        verified: dict[str, object] = {}
        for name in POSITIVE:
            expected = (args.fixtures / f"{name}.BIN").read_bytes()
            actual = (args.outputs / f"{name}.BIN").read_bytes()
            packed = (args.fixtures / f"{name}.RAW").read_bytes()
            if actual != expected:
                raise SystemExit(f"{name}: physical decoded bytes differ")
            if zlib.decompress(packed, -15) != expected:
                raise SystemExit(f"{name}: host zlib fixture decode differs")
            item = manifest["positive"][name]
            if len(actual) != item["size"] or f"{zlib.crc32(actual) & 0xffffffff:08x}" != item["crc32"]:
                raise SystemExit(f"{name}: size/CRC differs from manifest")
            verified[name] = {
                "packed_size": len(packed),
                "output_size": len(actual),
                "crc32": item["crc32"],
                "first_block_type": item["first_block_type"],
            }
        report["host_byte_oracle"] = "pass"
        report["verified"] = verified

    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
