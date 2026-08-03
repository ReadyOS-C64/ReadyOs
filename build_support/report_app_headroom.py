#!/usr/bin/env python3
"""Emit ReadyOS app memory/headroom facts from cc65 linker maps.

This is intentionally a report, not a pass/fail gate.  The gate remains
verify_memory_map.py.  This file gives REU refactor work a stable artifact for
before/after comparison before bank allocation authority or micromodule layout
changes are made.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import verify_memory_map as memmap


ROOT = Path(__file__).resolve().parents[1]
APP_WINDOW_END = 0xC7FF


def hex4(value: int) -> str:
    return f"0x{value:04X}"


def readyshell_heap_report(txt: str, segs: dict[str, tuple[int, int, int]]):
    if "BSS" not in segs:
        return None
    try:
        overlay_loadaddr = memmap.parse_map_symbol(txt, "__OVERLAY_LOADADDR__")
        overlay_start = memmap.parse_map_symbol(txt, "__OVERLAYSTART__")
        himem = memmap.parse_map_symbol(txt, "__HIMEM__")
    except ValueError:
        return None

    bss_end = segs["BSS"][1]
    heap_start = bss_end + 1
    if heap_start & 1:
        heap_start += 1
    heap_end = overlay_loadaddr - 1
    heap_size = heap_end - heap_start + 1 if heap_end >= heap_start else 0
    overlay_window = himem - overlay_start
    return {
        "heap_start": hex4(heap_start),
        "heap_end": hex4(heap_end),
        "heap_size": heap_size,
        "overlay_loadaddr": hex4(overlay_loadaddr),
        "overlay_start": hex4(overlay_start),
        "himem": hex4(himem),
        "overlay_window": overlay_window,
    }


def map_report(root: Path, rel: str, spec: dict, app_window_end: int) -> dict:
    path = root / rel
    txt, segs = memmap.parse_map_segments(path)
    main_names = spec["main_segments"]
    used_segments = {
        name: {
            "start": hex4(start),
            "end": hex4(end),
            "size": size,
        }
        for name, (start, end, size) in segs.items()
        if name in main_names
    }
    max_end = max(end for name, (_start, end, _size) in segs.items() if name in main_names)
    total_size = sum(size for name, (_start, _end, size) in segs.items() if name in main_names)
    code_names = ("STARTUP", "LOWCODE", "CODE", "RODATA", "INIT", "ONCE", "ENTRY", "RESIDENT", "HIDDEN", "BRIDGE")
    code_size = sum(segs[name][2] for name in code_names if name in segs)
    data_size = segs.get("DATA", (0, 0, 0))[2]
    bss_size = segs.get("BSS", (0, 0, 0))[2]
    report = {
        "name": Path(rel).stem,
        "map": rel,
        "runtime_end": hex4(max_end),
        "app_window_end": hex4(app_window_end),
        "app_window_headroom": app_window_end - max_end,
        "main_segment_total": total_size,
        "code_ro_init_bytes": code_size,
        "data_bytes": data_size,
        "bss_bytes": bss_size,
        "segments": used_segments,
    }
    if "readyshell" in Path(rel).stem:
        heap = readyshell_heap_report(txt, segs)
        if heap:
            report["readyshell"] = heap
            report["heap_capacity"] = heap["heap_size"]
    elif "BSS" in segs:
        try:
            himem = memmap.parse_map_symbol(txt, "__HIMEM__")
            stack_size = memmap.parse_map_symbol(txt, "__STACKSIZE__")
            heap_start = segs["BSS"][1] + 1
            if heap_start & 1:
                heap_start += 1
            heap_end = himem - stack_size - 1
            report["heap_capacity"] = max(0, heap_end - heap_start + 1)
            report["heap_start"] = hex4(heap_start)
            report["heap_end"] = hex4(heap_end)
            report["stack_reserve"] = stack_size
        except ValueError:
            pass
    return report


def comparison_markdown(before: dict, after: dict) -> str:
    old_by_name = {row["name"]: row for row in before["maps"]}
    lines = [
        "# ReadyOS Bank Refactor Memory Comparison",
        "",
        "Generated from linker maps before and after the schema-v5 ReadyOS-bank refactor.",
        "Positive headroom/heap deltas mean more free C64 RAM; negative code/data/BSS deltas mean smaller linked segments.",
        "",
        "| App/map | Code/RO/init before | after | delta | Data before | after | delta | BSS before | after | delta | Heap before | after | delta | Window headroom before | after | delta |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for new in after["maps"]:
        old = old_by_name.get(new["name"])
        if not old:
            continue

        def val(row: dict, key: str):
            return row.get(key)

        def cell(value):
            return "—" if value is None else str(value)

        def delta(key: str):
            a, b = val(old, key), val(new, key)
            return None if a is None or b is None else b - a

        lines.append(
            f"| {new['name']} | {cell(val(old, 'code_ro_init_bytes'))} | {cell(val(new, 'code_ro_init_bytes'))} | {cell(delta('code_ro_init_bytes'))} "
            f"| {cell(val(old, 'data_bytes'))} | {cell(val(new, 'data_bytes'))} | {cell(delta('data_bytes'))} "
            f"| {cell(val(old, 'bss_bytes'))} | {cell(val(new, 'bss_bytes'))} | {cell(delta('bss_bytes'))} "
            f"| {cell(val(old, 'heap_capacity'))} | {cell(val(new, 'heap_capacity'))} | {cell(delta('heap_capacity'))} "
            f"| {cell(val(old, 'app_window_headroom'))} | {cell(val(new, 'app_window_headroom'))} | {cell(delta('app_window_headroom'))} |"
        )
    lines.extend([
        "",
        f"Before snapshot end: `{before['app_window_end']}`. After snapshot end: `{after['app_window_end']}`.",
        "ReadyBASIC has no conventional cc65 BSS/heap: its custom assembler/linker budget is enforced separately by `verify_readybasic_plugin.py`.",
        "ReadyShell heap is bounded by its unchanged overlay load address, while its full snapshot headroom includes the new app-private `$C600-$C7FF` tail.",
        "",
    ])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        help="Optional JSON output path. Prints to stdout when omitted.",
    )
    parser.add_argument("--root", type=Path, default=ROOT,
                        help="Repository root containing the map files.")
    parser.add_argument("--app-window-end", type=lambda s: int(s, 0), default=APP_WINDOW_END,
                        help="Inclusive snapshot window end (default: 0xC7FF).")
    parser.add_argument("--compare-json", type=Path,
                        help="Earlier JSON report to compare with this report.")
    parser.add_argument("--markdown-output", type=Path,
                        help="Write a Markdown comparison (requires --compare-json).")
    args = parser.parse_args()

    spec = json.loads(memmap.SPEC_PATH.read_text(encoding="utf-8"))
    reports = []
    for rel in spec["map_files"]:
        path = args.root / rel
        if not path.exists():
            print(f"missing map file: {rel}", file=sys.stderr)
            return 1
        reports.append(map_report(args.root, rel, spec, args.app_window_end))

    payload = {
        "app_window_start": hex4(int(str(spec["ram_windows"]["app_runtime"]["start"]), 0)),
        "app_window_end": hex4(args.app_window_end),
        "maps": reports,
    }
    text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    if args.markdown_output:
        if not args.compare_json:
            parser.error("--markdown-output requires --compare-json")
        before = json.loads(args.compare_json.read_text(encoding="utf-8"))
        args.markdown_output.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_output.write_text(comparison_markdown(before, payload), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
