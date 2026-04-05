#!/usr/bin/env python3
"""
editor_host_smoke.py

Focused host-side checks for ReadyOS Editor:
- Editor source contracts for help, paging, find, and selection support
- Runtime headroom gate for obj/editor.map
- Viewport, paging, search, and selection/paste behavior smoke tests
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EDITOR_SRC = ROOT / "src" / "apps" / "editor" / "editor.c"
EDITOR_MAP = ROOT / "obj" / "editor.map"
APP_SNAPSHOT_END = 0xC5FF
EDIT_HEIGHT = 20
MIN_HEADROOM = 12 * 1024


def check(name: str, cond: bool, detail: str = "") -> bool:
    status = "OK" if cond else "FAIL"
    msg = f"[{status}] {name}"
    if detail:
        msg += f" - {detail}"
    print(msg)
    return cond


def parse_map_segments(path: Path) -> dict[str, tuple[int, int, int]]:
    txt = path.read_text(encoding="utf-8", errors="replace")
    seg_re = re.compile(
        r"^\s*([A-Z0-9_]+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+[0-9A-F]{5}\s*$",
        flags=re.MULTILINE,
    )
    segments: dict[str, tuple[int, int, int]] = {}
    in_list = False
    for line in txt.splitlines():
        if line.strip() == "Segment list:":
            in_list = True
            continue
        if not in_list:
            continue
        match = seg_re.match(line)
        if match:
            segments[match.group(1)] = (
                int(match.group(2), 16),
                int(match.group(3), 16),
                int(match.group(4), 16),
            )
            continue
        if segments and not line.strip():
            break
    if not segments:
        raise ValueError(f"segment list missing: {path}")
    return segments


def max_scroll(line_count: int) -> int:
    return max(0, line_count - EDIT_HEIGHT)


def clamp_view(line_count: int, cursor_x: int, cursor_y: int, scroll_y: int, lengths: list[int]) -> tuple[int, int, int]:
    if line_count <= 0:
        line_count = 1
    cursor_y = min(cursor_y, line_count - 1)
    cursor_x = min(cursor_x, max(0, lengths[cursor_y]))
    scroll_y = min(scroll_y, max_scroll(line_count))
    if cursor_y < scroll_y:
        scroll_y = cursor_y
    elif cursor_y >= scroll_y + EDIT_HEIGHT:
        scroll_y = cursor_y - EDIT_HEIGHT + 1
        scroll_y = min(scroll_y, max_scroll(line_count))
    return cursor_x, cursor_y, scroll_y


def page_down(line_count: int, cursor_y: int, scroll_y: int) -> tuple[int, int]:
    jump = EDIT_HEIGHT - 1
    cursor_y = min(line_count - 1, cursor_y + jump)
    scroll_y = min(max_scroll(line_count), scroll_y + jump)
    _, cursor_y, scroll_y = clamp_view(line_count, 0, cursor_y, scroll_y, [0] * line_count)
    return cursor_y, scroll_y


def page_up(line_count: int, cursor_y: int, scroll_y: int) -> tuple[int, int]:
    jump = EDIT_HEIGHT - 1
    cursor_y = max(0, cursor_y - jump)
    scroll_y = max(0, scroll_y - jump)
    _, cursor_y, scroll_y = clamp_view(line_count, 0, cursor_y, scroll_y, [0] * line_count)
    return cursor_y, scroll_y


def find_next(lines: list[str], cursor_x: int, cursor_y: int, needle: str) -> tuple[int, int] | None:
    needle_l = needle.lower()
    if not needle_l:
        return None

    for line_idx in range(cursor_y, len(lines)):
        start = cursor_x + 1 if line_idx == cursor_y else 0
        pos = lines[line_idx].lower().find(needle_l, start)
        if pos != -1:
            return pos, line_idx

    for line_idx in range(0, cursor_y + 1):
        pos = lines[line_idx].lower().find(needle_l)
        if pos != -1:
            if line_idx == cursor_y and pos <= cursor_x:
                continue
            return pos, line_idx

    return None


def selection_bounds(anchor_x: int, anchor_y: int, cursor_x: int, cursor_y: int) -> tuple[int, int, int, int]:
    if anchor_y < cursor_y or (anchor_y == cursor_y and anchor_x <= cursor_x):
        return anchor_y, anchor_x, cursor_y, cursor_x
    return cursor_y, cursor_x, anchor_y, anchor_x


def serialize_selection(lines: list[str], anchor_x: int, anchor_y: int, cursor_x: int, cursor_y: int) -> str:
    start_y, start_x, end_y, end_x = selection_bounds(anchor_x, anchor_y, cursor_x, cursor_y)
    out: list[str] = []
    for line_idx in range(start_y, end_y + 1):
        from_col = start_x if line_idx == start_y else 0
        to_col = end_x if line_idx == end_y else len(lines[line_idx])
        out.append(lines[line_idx][from_col:to_col])
        if line_idx != end_y:
            out.append("\r")
    return "".join(out)


def apply_paste(lines: list[str], cursor_x: int, cursor_y: int, payload: str,
                max_lines: int = 100, max_line_len: int = 80) -> tuple[list[str], int, int]:
    out = list(lines)
    for ch in payload:
        if ch == "\r":
            if len(out) >= max_lines:
                break
            current = out[cursor_y]
            out[cursor_y] = current[:cursor_x]
            out.insert(cursor_y + 1, current[cursor_x:])
            cursor_y += 1
            cursor_x = 0
            continue

        if len(out[cursor_y]) >= max_line_len - 1:
            continue

        current = out[cursor_y]
        out[cursor_y] = current[:cursor_x] + ch + current[cursor_x:]
        cursor_x += 1

    return out, cursor_x, cursor_y


def main() -> int:
    ok = True

    print("=== Editor Source Contract ===")
    src = EDITOR_SRC.read_text(encoding="utf-8", errors="replace")
    ok &= check("editor has help popup", "show_help_popup(" in src)
    ok &= check("editor has find dialog", "show_find_dialog(" in src)
    ok &= check("editor has find-next logic", "find_next_match(" in src)
    ok &= check("editor has page down shortcut", "KEY_CTRL_N" in src)
    ok &= check("editor has page up shortcut", "KEY_CTRL_P" in src)
    ok &= check("editor has selection toggle", "KEY_SELECT" in src)
    ok &= check("editor has save-as ctrl shortcut", "KEY_CTRL_A" in src)
    ok &= check("editor has selection cursor handler", "handle_cursor_selection(" in src)
    ok &= check("editor uses safe text column width", "#define LINE_DISPLAY (40 - TEXT_X)" in src)
    ok &= check("help mentions selection", "F6:SELECT" in src and "CTRL+A:SAVE AS" in src)

    print("\n=== Editor Headroom Gate ===")
    try:
        segments = parse_map_segments(EDITOR_MAP)
        runtime_end = max(
            segments[name][1]
            for name in ("STARTUP", "CODE", "RODATA", "DATA", "INIT", "ONCE", "BSS")
            if name in segments
        )
        headroom = APP_SNAPSHOT_END - runtime_end
        ok &= check("editor runtime headroom", headroom >= MIN_HEADROOM, f"{headroom} bytes")
    except Exception as ex:  # pragma: no cover
        ok &= check("editor map parse", False, str(ex))

    print("\n=== Viewport Smoke ===")
    lengths = [1] * 30
    cursor_x, cursor_y, scroll_y = clamp_view(30, 0, 29, 0, lengths)
    ok &= check("bottom line becomes visible", (cursor_y, scroll_y) == (29, 10), f"{cursor_y},{scroll_y}")
    cursor_y, scroll_y = page_down(30, 0, 0)
    ok &= check("page down moves viewport", (cursor_y, scroll_y) == (19, 10), f"{cursor_y},{scroll_y}")
    cursor_y, scroll_y = page_up(30, 29, 10)
    ok &= check("page up moves viewport", (cursor_y, scroll_y) == (10, 0), f"{cursor_y},{scroll_y}")

    print("\n=== Search Smoke ===")
    sample = ["alpha", "beta gamma", "Alpha beta", "omega"]
    ok &= check("find hits later in file", find_next(sample, 0, 0, "gamma") == (5, 1))
    ok &= check("find is case-insensitive", find_next(sample, 0, 0, "alpha") == (0, 2))
    ok &= check("find wraps once", find_next(sample, 3, 3, "beta") == (0, 1))
    ok &= check("missing term reports none", find_next(sample, 0, 0, "delta") is None)

    print("\n=== Selection Smoke ===")
    sample = ["abcd", "ef", "ghi"]
    ok &= check("forward selection serializes", serialize_selection(sample, 1, 0, 2, 2) == "bcd\ref\rgh")
    ok &= check("reverse selection normalizes", serialize_selection(sample, 2, 2, 1, 0) == "bcd\ref\rgh")
    ok &= check("newline-only selection serializes", serialize_selection(sample, 4, 0, 0, 1) == "\r")
    pasted, px, py = apply_paste(["ab", "cd"], 0, 1, "xy\rzz")
    ok &= check("multiline paste preserves line breaks", pasted == ["ab", "xy", "zzcd"], f"{pasted!r}")
    ok &= check("paste updates cursor position", (px, py) == (2, 2), f"{px},{py}")

    print("\nALL CHECKS PASSED" if ok else "\nSOME CHECKS FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
