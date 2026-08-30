#!/usr/bin/env python3
"""Validate physical xuzio memory evidence and downloaded filesystem output."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import zlib


CASES = (
    ("S00000.BIN", 0),
    ("S00001.BIN", 1),
    ("S00511.BIN", 511),
    ("S00512.BIN", 512),
    ("S00513.BIN", 513),
    ("S04095.BIN", 4095),
    ("S04096.BIN", 4096),
    ("S65535.BIN", 65535),
    ("S65536.BIN", 65536),
)


def run_dir_from_log(path: pathlib.Path) -> pathlib.Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return pathlib.Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: pathlib.Path) -> pathlib.Path:
    candidates = [
        path for path in run_dir.rglob("*xuzio_results_c000*.bin") if path.is_file()
    ]
    if not candidates:
        raise SystemExit(f"no xuzio C64 memory dump below {run_dir}")
    return max(candidates, key=lambda path: path.stat().st_mtime)


def pattern(index: int, position: int) -> int:
    return (
        (position & 0xFF)
        ^ ((position >> 8) & 0xFF)
        ^ ((position >> 16) & 0xFF)
        ^ ((index * 29) & 0xFF)
        ^ 0xA5
    )


def expected_bytes(index: int, size: int) -> bytes:
    return bytes(pattern(index, position) for position in range(size))


def u16(data: bytes, offset: int) -> int:
    return data[offset] | (data[offset + 1] << 8)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_log", type=pathlib.Path)
    parser.add_argument("downloads", type=pathlib.Path)
    parser.add_argument("--json-output", type=pathlib.Path)
    parser.add_argument("--probe-only", action="store_true")
    args = parser.parse_args()

    run_dir = run_dir_from_log(args.run_log)
    dump_path = find_dump(run_dir)
    result = dump_path.read_bytes()
    magic_at = result.find(b"XUZ1")
    if magic_at < 0:
        raise SystemExit(f"{dump_path}: missing XUZ1 result magic")
    result = result[magic_at:magic_at + 128]
    if len(result) < 128:
        raise SystemExit(f"{dump_path}: short result block")
    if result[4] != 1 or result[5] != 1 or result[6] != 8 or result[7] != 0:
        root_summary = result[72:112].split(b"\0", 1)[0].decode(
            "ascii", errors="replace"
        )
        usb_summary = result[32:68].split(b"\0", 1)[0].decode(
            "ascii", errors="replace"
        )
        raise SystemExit(
            f"C64 probe failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} code=${result[7]:02X} "
            f"case={result[8]} path_step={result[18]} root_count={result[19]} "
            f"usb_count={result[20]} usb_attr=${result[21]:02X} "
            f"io_step={result[22]} reported_size={int.from_bytes(result[24:28], 'little')} "
            f"last_got={int.from_bytes(result[28:30], 'little', signed=True)} "
            f"actual=${result[30]:02X} expected=${result[31]:02X} "
            f"reader=(${result[14]:02X},${result[15]:02X}) "
            f"writer=(${result[16]:02X},${result[17]:02X}) "
            f"root={root_summary!r} usb_readyos={usb_summary!r}"
        )
    if result[9] != len(CASES) or u16(result, 10) != len(CASES) + 1:
        raise SystemExit("C64 probe did not drain the expected complete directory")
    if u16(result, 12) not in (0xDF1C, 0xDE1C, 0xDFFC):
        raise SystemExit(f"unexpected UCI base ${u16(result, 12):04X}")
    if result[14] != 0 or result[16] != 0:
        raise SystemExit(
            f"final transport flags reader=${result[14]:02X} writer=${result[16]:02X}"
        )

    base_report = {
        "physical_result": "pass",
        "version": result[4],
        "stage": result[6],
        "uci_base": f"{u16(result, 12):04x}",
        "directory_count": u16(result, 10),
        "dump": str(dump_path),
        "physical_run_dir": str(run_dir),
    }
    if args.probe_only:
        rendered = json.dumps(base_report, indent=2, sort_keys=True)
        print(rendered)
        if args.json_output:
            args.json_output.write_text(rendered + "\n", encoding="utf-8")
        return 0

    files = []
    for index, (name, size) in enumerate(CASES):
        path = args.downloads / name
        data = path.read_bytes()
        expected = expected_bytes(index, size)
        if data != expected:
            raise SystemExit(f"{path}: content mismatch at size {size}")
        host_crc = zlib.crc32(data) & 0xFFFFFFFF
        c64_crc = int.from_bytes(result[32 + index * 4:36 + index * 4], "little")
        if host_crc != c64_crc:
            raise SystemExit(
                f"{path}: host CRC ${host_crc:08X} != C64 CRC ${c64_crc:08X}"
            )
        files.append({"name": name, "size": size, "crc32": f"{host_crc:08x}"})

    renamed = (args.downloads / "RENAMED.OK").read_bytes()
    if renamed != b"\x5a":
        raise SystemExit("RENAMED.OK does not contain the exact C64 byte")

    report = {
        **base_report,
        "files": files,
        "renamed": "RENAMED.OK",
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
