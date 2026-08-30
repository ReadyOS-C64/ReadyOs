#!/usr/bin/env python3
"""Validate physical fixed-Deflate output and independent raw-zlib oracles."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import zlib


CASES = ("EMPTY", "REPEAT", "RANDOM", "CROSS")


def run_dir_from_log(path: Path) -> Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: Path) -> Path:
    candidates = [
        path for path in run_dir.rglob("*xuzdeflate_results_033c*.bin")
        if path.is_file()
    ]
    if not candidates:
        raise SystemExit(f"no xuzdeflate result dump below {run_dir}")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def u16(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8)


def jiffy(data: bytes, offset: int) -> int:
    return (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]


def decode_raw(data: bytes) -> bytes:
    decoder = zlib.decompressobj(-15)
    plain = decoder.decompress(data) + decoder.flush()
    if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise SystemExit("raw Deflate stream is truncated or has trailing bytes")
    return plain


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_log", type=Path)
    parser.add_argument("fixtures", type=Path)
    parser.add_argument("--outputs", type=Path)
    parser.add_argument("--probe-only", action="store_true")
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args()

    run_dir = (args.run_log.expanduser().resolve() if args.run_log.is_dir()
               else run_dir_from_log(args.run_log))
    dump_path = find_dump(run_dir)
    dump = dump_path.read_bytes()
    at = dump.find(b"XZD1")
    if at < 0 or len(dump) - at < 128:
        raise SystemExit(f"{dump_path}: missing complete XZD1 result")
    result = dump[at:at + 128]
    expected_active = 7 if result[4] == 3 else 4
    if (result[4] not in (1, 2, 3) or
            result[5:9] != bytes((1, 7, 0, 4)) or
            result[9] != expected_active):
        raise SystemExit(
            f"C64 xuzdeflate failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} code=${result[7]:02X} complete={result[8]} "
            f"active={result[9]} deflate={result[18]} "
            f"input=(${result[14]:02X},${result[15]:02X}) "
            f"output=(${result[16]:02X},${result[17]:02X}) "
            f"trace=(${result[124]:02X},${result[125]:02X},"
            f"${result[126]:02X},${result[127]:02X})"
        )
    if result[20] != 4 or result[19] != 1 or result[21] != 0xA5 or result[22] != 0:
        raise SystemExit("case count, scratch release, or workspace guard failed")
    if result[4] >= 2 and result[23] != 1:
        raise SystemExit("catalog bank was not released")
    if result[12] == 0xFF or result[13] == 0xFF or result[12] == result[13]:
        raise SystemExit("invalid package/work physical bank IDs")
    uci = u16(result, 10)
    if uci not in (0xDF1C, 0xDE1C, 0xDFFC):
        raise SystemExit(f"invalid UCI base ${uci:04X}")
    if result[14] or result[16] or result[18]:
        raise SystemExit("transport or compressor error remains in result")
    stack_initial = u16(result, 112)
    stack_low = u16(result, 114)
    if not (0xC400 <= stack_low <= stack_initial <= 0xC600):
        raise SystemExit(
            f"compressor stack escaped reserved window: "
            f"initial=${stack_initial:04X} low=${stack_low:04X}"
        )
    if result[116:118] != bytes((7, 6)):
        raise SystemExit("packed codec package version/phase count mismatch")
    if result[122:124] != bytes((8, 8)):
        raise SystemExit(
            f"final CROSS phase sequence was {result[122]}/{result[123]}, expected 8/8"
        )

    manifest = json.loads((args.fixtures / "manifest.json").read_text())
    timing: dict[str, object] = {}
    c64_cases: dict[str, object] = {}
    for index, name in enumerate(CASES):
        item = manifest["cases"][name]
        expected_size = int(item["size"])
        expected_crc = int(item["crc32"], 16)
        input_size = int.from_bytes(result[96 + 4 * index:100 + 4 * index], "little")
        output_size = int.from_bytes(result[48 + 4 * index:52 + 4 * index], "little")
        crc = int.from_bytes(result[64 + 4 * index:68 + 4 * index], "little")
        fixed = u16(result, 80 + 2 * index)
        stored = u16(result, 88 + 2 * index)
        blocks = 1 if expected_size == 0 else (expected_size + 2047) // 2048
        if input_size != expected_size or crc != expected_crc:
            raise SystemExit(f"{name}: C64 input size/CRC differs from fixture")
        if fixed + stored != blocks:
            raise SystemExit(
                f"{name}: C64 block counts {fixed}+{stored} != {blocks}"
            )
        if name == "EMPTY" and (fixed, stored) != (1, 0):
            raise SystemExit("EMPTY was not one final fixed block")
        if name == "REPEAT" and (fixed == 0 or stored != 0):
            raise SystemExit("REPEAT did not select fixed blocks exclusively")
        if name == "RANDOM" and stored == 0:
            raise SystemExit("RANDOM did not exercise stored fallback")
        if name == "CROSS" and (fixed == 0 or stored == 0):
            raise SystemExit("CROSS did not exercise both stored and fixed blocks")
        start = jiffy(result, 24 + 3 * index)
        end = jiffy(result, 36 + 3 * index)
        ticks = (end - start) & 0xFFFFFF
        timing[name] = {"jiffies": ticks, "seconds_at_60hz": round(ticks / 60, 3)}
        c64_cases[name] = {
            "input_size": input_size,
            "output_size": output_size,
            "crc32": f"{crc:08x}",
            "fixed_blocks": fixed,
            "stored_blocks": stored,
        }

    report: dict[str, object] = {
        "physical_result": "pass",
        "physical_run_dir": str(run_dir),
        "dump": str(dump_path),
        "uci_base": f"{uci:04x}",
        "package_bank": result[12],
        "work_bank": result[13],
        "work_bank_released": "pass",
        "workspace_guard": f"{result[21]:02x}",
        "cc65_software_stack": {
            "initial": f"{stack_initial:04x}",
            "low_water": f"{stack_low:04x}",
            "bytes_below_initial": stack_initial - stack_low,
            "reserved_floor": "c400",
        },
        "timing": timing,
        "c64_cases": c64_cases,
    }
    if result[4] >= 2:
        report["catalog_bank_released"] = "pass"
    if result[4] == 3:
        report["mixed_archive_entries"] = 7
    if not args.probe_only:
        if args.outputs is None:
            raise SystemExit("--outputs is required for the byte oracle")
        verified: dict[str, object] = {}
        for name in CASES:
            raw = (args.outputs / f"{name}.RAW").read_bytes()
            expected = (args.fixtures / f"{name}.BIN").read_bytes()
            plain = decode_raw(raw)
            if plain != expected:
                raise SystemExit(f"{name}: physical raw stream decodes to different bytes")
            index = CASES.index(name)
            if len(raw) != c64_cases[name]["output_size"]:
                raise SystemExit(f"{name}: downloaded raw size differs from C64 result")
            first_type = (raw[0] >> 1) & 3 if raw else -1
            expected_first = 0 if name in ("RANDOM", "CROSS") else 1
            if first_type != expected_first:
                raise SystemExit(
                    f"{name}: first block type {first_type}, expected {expected_first}"
                )
            verified[name] = {
                "raw_size": len(raw),
                "plain_size": len(plain),
                "plain_crc32": f"{zlib.crc32(plain) & 0xFFFFFFFF:08x}",
                "first_block_type": first_type,
            }
        report["host_raw_zlib_oracle"] = "pass"
        report["verified"] = verified

    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
