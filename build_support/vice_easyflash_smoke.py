#!/usr/bin/env python3
"""
VICE smoke verification helpers for the ReadyOS EasyFlash flavor.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Dict


ROOT = Path(__file__).resolve().parents[1]
BIN_DIR = ROOT / "bin"

EXPECTED_OVL_META = bytes([0x4F, 0x56, 0x02, 0xFF, 0x40, 0x41, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00])


def fail(message: str) -> None:
    raise ValueError(message)


def payload_bytes_from_prg(path: Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) < 3:
        fail(f"payload too short: {path}")
    return raw[2:]


def expected_bitmap(layout: Dict[str, object]) -> bytes:
    bitmap = [0, 0, 0]
    for entry in layout["apps"]:
        bank = int(entry["bank"])
        if bank >= 24:
            fail(f"unsupported launcher bank for smoke bitmap: {bank}")
        byte_index = bank // 8
        bit_index = bank % 8
        bitmap[byte_index] |= 1 << bit_index
    return bytes(bitmap + [0x08])


def parse_monitor_log(path: Path) -> Dict[int, bytes]:
    dumps: Dict[int, bytes] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"^>C:([0-9a-fA-F]{4})\s+(.*)$", line)
        if not match:
            continue
        addr = int(match.group(1), 16)
        # VICE appends an ASCII rendering after the hex bytes. Parse only the
        # leading monitor byte column so PETSCII-looking text cannot be
        # misread as extra dumped bytes.
        values: list[str] = []
        for token in match.group(2).split():
            if not re.fullmatch(r"[0-9a-fA-F]{2}", token):
                break
            values.append(token)
        if not values:
            continue
        dumps[addr] = bytes(int(value, 16) for value in values)
    return dumps


def region_bytes(dumps: Dict[int, bytes], start: int, length: int) -> bytes:
    out = bytearray()
    cursor = start
    while len(out) < length:
        chunk = dumps.get(cursor)
        if not chunk:
            break
        out.extend(chunk)
        cursor += len(chunk)
    return bytes(out[:length])


def check_prefix(actual: bytes, expected: bytes, label: str) -> None:
    if actual[:len(expected)] != expected:
        fail(
            f"{label} mismatch: expected {expected.hex(' ')}, "
            f"saw {actual[:len(expected)].hex(' ')}"
        )


def marker_present(ring: bytes, marker: bytes) -> bool:
    value = marker[0]
    return value in ring or (value | 0x80) in ring


def symbol_value_from_map(path: Path, symbol: str) -> int:
    pattern = re.compile(rf"(?:^|\s){re.escape(symbol)}\s+([0-9A-Fa-f]{{6}})\s+RLA", re.M)
    text = path.read_text(encoding="utf-8", errors="replace")
    match = pattern.search(text)
    if not match:
        fail(f"symbol {symbol} not found in map {path}")
    return int(match.group(1), 16) & 0xFFFF


def optional_symbol_value_from_map(path: Path, symbol: str) -> int | None:
    pattern = re.compile(rf"(?:^|\s){re.escape(symbol)}\s+([0-9A-Fa-f]{{6}})\s+RLA", re.M)
    text = path.read_text(encoding="utf-8", errors="replace")
    match = pattern.search(text)
    if not match:
        return None
    return int(match.group(1), 16) & 0xFFFF


def write_monitor(output_path: Path,
                  launcher_map_path: Path,
                  boot_map_path: Path | None = None,
                  include_preload: bool = False) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    main_addr = symbol_value_from_map(launcher_map_path, "_main")
    keyloop_addr = symbol_value_from_map(launcher_map_path, "_tui_getkey")
    callmain_addr = symbol_value_from_map(launcher_map_path, "callmain")
    initlib_addr = symbol_value_from_map(launcher_map_path, "initlib")
    zerobss_addr = symbol_value_from_map(launcher_map_path, "zerobss")
    optional_launcher_breaks = [
        optional_symbol_value_from_map(launcher_map_path, "_ef_linit_entry"),
        optional_symbol_value_from_map(launcher_map_path, "_ef_linit_before_bootstrap"),
        optional_symbol_value_from_map(launcher_map_path, "_ef_linit_after_bootstrap"),
        optional_symbol_value_from_map(launcher_map_path, "_ef_linit_before_catalog_defaults"),
        optional_symbol_value_from_map(launcher_map_path, "_ef_linit_after_catalog_defaults"),
    ]
    lines = []
    if include_preload:
        if boot_map_path is None:
            fail("--include-preload requires --boot-map")
        shim_addr = symbol_value_from_map(boot_map_path, "easyflash_shim_copy_done")
        preload_addr = symbol_value_from_map(boot_map_path, "easyflash_preload_verify_done")
        kernal_addr = symbol_value_from_map(boot_map_path, "easyflash_after_kernal_init")
        restore_addr = symbol_value_from_map(boot_map_path, "easyflash_after_launcher_restore")
        jump_addr = symbol_value_from_map(boot_map_path, "easyflash_before_launcher_jump")
        lines.append(f"break {shim_addr:04x}")
        lines.append(f"break {preload_addr:04x}")
        lines.append(f"break {kernal_addr:04x}")
        lines.append(f"break {restore_addr:04x}")
        lines.append(f"break {jump_addr:04x}")
    lines.extend([
        "break 1000",
        "break 890d",
        f"break {initlib_addr:04x}",
        f"break {zerobss_addr:04x}",
        f"break {callmain_addr:04x}",
        f"break {main_addr:04x}",
        f"break {keyloop_addr:04x}",
        "x",
    ])
    for addr in optional_launcher_breaks:
        if addr is not None:
            lines.insert(-1, f"break {addr:04x}")
    if include_preload:
        lines.extend([
            "m 0f20 112f",
            "m c800 c9ff",
            "x",
            "m 1000 107f",
            "m ca00 cb1f",
            "m c600 c9ff",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 1000 107f",
            "m c7a0 c7ef",
            "m c836 c839",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 1000 107f",
            "m c7a0 c7ef",
            "m c836 c839",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 1000 107f",
            "m 88e0 892f",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 88e0 892f",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 88e0 892f",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 8927 89a6",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 7c50 7c7f",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m 0400 047f",
            "m d800 d87f",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
            "m c7a0 c7ef",
            "m 0000 001b",
            "m de00 de02",
            "m df01 df08",
            "x",
        ])
    lines.extend([
        "m 1000 101f",
        "m c7a0 c7ef",
        "m c600 c9ff",
        "m de00 de02",
        "m df01 df08",
        "x",
        "m 0400 047f",
        "m d800 d87f",
        "m c7a0 c7ef",
        "m c700 c700",
        "m c836 c839",
        "m c7f0 c7fb",
        "m de00 de02",
        "m df01 df08",
        "q",
    ])
    mon_cmds = "\n".join(lines) + "\n"
    output_path.write_text(mon_cmds, encoding="ascii")


def expected_preload_records(layout: Dict[str, object]) -> list[tuple[int, int, int]]:
    records: list[tuple[int, int, int]] = [
        (ord("L"), 0, int(layout["launcher"]["checksum16"])),
    ]
    for entry in layout["apps"]:
        records.append((ord("A"), int(entry["bank"]), int(entry["checksum16"])))
    for index, entry in enumerate(layout["overlays"], start=1):
        records.append((ord("O"), index, int(entry["checksum16"])))
    return records


def verify_preload_diagnostics(dumps: Dict[int, bytes], layout: Dict[str, object]) -> None:
    diag = region_bytes(dumps, 0xCA00, 0x120)
    if len(diag) >= 0x30 and diag[:4] == b"EFV\x01":
        expected = expected_preload_records(layout)
        total = diag[4]
        fails = diag[5]
        if total != len(expected):
            fail(f"preload diagnostic count mismatch: expected {len(expected)}, saw {total}")
        if fails != 0:
            fail(f"preload diagnostic reported {fails} failing payload(s)")

        base = 0x20
        for offset, (kind, index, checksum) in enumerate(expected):
            record = diag[base + offset * 10:base + offset * 10 + 10]
            if len(record) < 10:
                fail(f"missing preload diagnostic record {offset}")
            actual_kind, actual_index, status = record[0], record[1], record[2]
            expected_lo = record[3] | (record[4] << 8)
            actual = record[5] | (record[6] << 8)
            if actual_kind != kind or actual_index != index:
                fail(
                    f"preload diagnostic record {offset} identity mismatch: "
                    f"expected {chr(kind)}:{index}, saw {chr(actual_kind)}:{actual_index}"
                )
            if expected_lo != checksum:
                fail(
                    f"preload diagnostic record {offset} expected-checksum mismatch: "
                    f"layout ${checksum:04X}, diagnostic ${expected_lo:04X}"
                )
            if status != 0 or actual != checksum:
                fail(
                    f"preload diagnostic record {offset} failed for {chr(kind)}:{index}: "
                    f"expected ${checksum:04X}, actual ${actual:04X}, status {status}"
                )
        return

    # Current boot ROM no longer writes the transient EFV checksum block at
    # $CA00. Fall back to the persistent preload state that survives into the
    # launcher handoff: REU init magic, app bitmap, overlay cache metadata, and
    # boot-stage debug markers up through V(erify).
    if 0xC700 not in dumps:
        fail("missing REU magic memory dump at $C700")
    if 0xC836 not in dumps:
        fail("missing launcher bitmap memory dump at $C836")
    if 0xC7F0 not in dumps:
        fail("missing ReadyShell overlay metadata dump at $C7F0")
    if 0xC7A0 not in dumps:
        fail("missing launcher debug ring dump at $C7A0")

    bitmap = expected_bitmap(layout)
    check_prefix(dumps[0xC700], bytes([0xA5]), "REU magic")
    check_prefix(dumps[0xC836], bitmap, "app preload bitmap/storage drive")
    check_prefix(dumps[0xC7F0], EXPECTED_OVL_META, "overlay metadata")
    ring = region_bytes(dumps, 0xC7A0, 0x50)
    for marker in (b"R", b"L", b"A", b"O", b"M", b"V"):
        if not marker_present(ring, marker):
            fail(f"preload debug ring missing marker {marker.decode('ascii')}")


def verify_log(output_dir: Path,
               layout_path: Path,
               log_path: Path,
               vice_rc: int,
               launcher_map_path: Path,
               verify_preload: bool = False,
               preload_only: bool = False) -> None:
    crt_path = output_dir / "readyos_easyflash.crt"
    d64_path = output_dir / "readyos_data.d64"
    if not crt_path.exists():
        fail(f"missing crt artifact: {crt_path}")
    if not d64_path.exists():
        fail(f"missing data disk artifact: {d64_path}")
    if not log_path.exists():
        fail(f"missing VICE monitor log: {log_path}")

    layout = json.loads(layout_path.read_text(encoding="utf-8"))
    log_text = log_path.read_text(encoding="utf-8", errors="replace")
    dumps = parse_monitor_log(log_path)
    if verify_preload:
        verify_preload_diagnostics(dumps, layout)
    if preload_only:
        print("easyflash preload verification passed")
        if vice_rc != 0:
            print(f"  note: VICE exited with rc={vice_rc}, but preload diagnostics verified successfully")
        return

    launcher_prefix = payload_bytes_from_prg(BIN_DIR / "launcher_easyflash.prg")[:32]
    bitmap = expected_bitmap(layout)
    main_addr = symbol_value_from_map(launcher_map_path, "_main")
    keyloop_addr = symbol_value_from_map(launcher_map_path, "_tui_getkey")

    if f"C:${main_addr:04x}" not in log_text and f"C:${main_addr:04X}" not in log_text:
        fail(f"launcher main breakpoint at ${main_addr:04X} was not reached (VICE rc={vice_rc})")
    if f"C:${keyloop_addr:04x}" not in log_text and f"C:${keyloop_addr:04X}" not in log_text:
        fail(f"launcher key-loop breakpoint at ${keyloop_addr:04X} was not reached (VICE rc={vice_rc})")

    if 0x1000 not in dumps:
        fail("missing launcher memory dump at $1000")
    if 0x0400 not in dumps:
        fail("missing launcher screen dump at $0400")
    if 0xC7A0 not in dumps:
        fail("missing launcher debug ring dump at $C7A0")
    if 0xC700 not in dumps:
        fail("missing REU magic memory dump at $C700")
    if 0xC836 not in dumps:
        fail("missing launcher bitmap memory dump at $C836")
    if 0xC7F0 not in dumps:
        fail("missing ReadyShell overlay metadata dump at $C7F0")

    check_prefix(region_bytes(dumps, 0x1000, len(launcher_prefix)), launcher_prefix, "launcher RAM prefix")
    check_prefix(dumps[0xC700], bytes([0xA5]), "REU magic")
    check_prefix(dumps[0xC836], bitmap, "app preload bitmap/storage drive")
    check_prefix(dumps[0xC7F0], EXPECTED_OVL_META, "overlay metadata")
    ring = region_bytes(dumps, 0xC7A0, 0x50)
    for marker in (b"R", b"L", b"A", b"O", b"M", b"V", b"T", b"J", b"K"):
        if not marker_present(ring, marker):
            fail(f"launcher debug ring missing marker {marker.decode('ascii')}")
    if region_bytes(dumps, 0x0400, 0x80).count(0x20) == 0x80:
        fail("launcher screen dump is still blank")

    print("easyflash smoke passed")
    print(f"  launcher main breakpoint: ${main_addr:04X}")
    print(f"  launcher key-loop breakpoint: ${keyloop_addr:04X}")
    print(f"  preload bitmap: {bitmap[:3].hex(' ')}")
    print(f"  storage drive: {bitmap[3]}")
    if vice_rc != 0:
        print(f"  note: VICE exited with rc={vice_rc}, but the monitor log verified successfully")


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)

    write_parser = sub.add_parser("write-monitor")
    write_parser.add_argument("--output", required=True)
    write_parser.add_argument("--launcher-map", required=True)
    write_parser.add_argument("--boot-map")
    write_parser.add_argument("--include-preload", action="store_true")

    verify_parser = sub.add_parser("verify-log")
    verify_parser.add_argument("--output-dir", required=True)
    verify_parser.add_argument("--layout", required=True)
    verify_parser.add_argument("--log", required=True)
    verify_parser.add_argument("--launcher-map", required=True)
    verify_parser.add_argument("--vice-rc", type=int, default=0)
    verify_parser.add_argument("--verify-preload", action="store_true")
    verify_parser.add_argument("--preload-only", action="store_true")

    args = ap.parse_args()

    try:
        if args.cmd == "write-monitor":
            write_monitor(
                Path(args.output),
                Path(args.launcher_map),
                Path(args.boot_map) if args.boot_map else None,
                bool(args.include_preload),
            )
            return 0
        if args.cmd == "verify-log":
            verify_log(
                output_dir=Path(args.output_dir),
                layout_path=Path(args.layout),
                log_path=Path(args.log),
                launcher_map_path=Path(args.launcher_map),
                vice_rc=args.vice_rc,
                verify_preload=bool(args.verify_preload),
                preload_only=bool(args.preload_only),
            )
            return 0
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
