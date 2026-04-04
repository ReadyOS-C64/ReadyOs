#!/usr/bin/env python3
"""
Build ReadyOS apps catalog as a strict lowercase-PETASCII SEQ payload.

Input source is human-editable UTF-8 text:
  drive:prg_name:display_name
  description line
  ...

Rules:
- Non-empty, non-comment lines are parsed as alternating entry/description lines.
- Source text must be lowercase for all alphabetic characters.
- PRG token must already be lowercase, plain filename token (no suffixes), and must not include ".prg".
- Output line bytes use PETASCII-lowercase encoding convention:
  alphabetic chars are emitted as ASCII uppercase byte codes (A-Z),
  which represent lowercase text semantics in C64 PETASCII flows.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from typing import List, Tuple


PRG_RE = re.compile(r"[a-z0-9_.-]+")


def fail(path: str, line_no: int, msg: str) -> None:
    raise ValueError(f"{path}:{line_no}: {msg}")


def has_upper_ascii(text: str) -> bool:
    for ch in text:
        if "A" <= ch <= "Z":
            return True
    return False


def normalize_prg_token(raw: str, path: str, line_no: int) -> str:
    prg = raw.strip()
    if not prg:
        fail(path, line_no, "empty PRG token")
    if has_upper_ascii(prg):
        fail(path, line_no, f"PRG token must be lowercase: {raw!r}")

    if "," in prg:
        fail(path, line_no, f"comma suffix not allowed in PRG token: {raw!r}")

    if prg.endswith(".prg"):
        fail(path, line_no, f".prg extension not allowed: {raw!r}")

    if len(prg) == 0 or len(prg) > 12:
        fail(path, line_no, f"PRG token length invalid: {raw!r}")
    if not PRG_RE.fullmatch(prg):
        fail(path, line_no, f"invalid PRG characters: {raw!r}")
    return prg


def parse_source(path: str) -> List[str]:
    with open(path, "r", encoding="utf-8", errors="strict") as f:
        raw_lines = f.read().splitlines()

    logical: List[Tuple[int, str]] = []
    for idx, raw in enumerate(raw_lines, start=1):
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if has_upper_ascii(line):
            fail(path, idx, "alphabetic text must be lowercase in source catalog")
        logical.append((idx, line))

    out_lines: List[str] = []
    i = 0
    while i < len(logical):
        entry_no, entry = logical[i]
        i += 1

        parts = [p.strip() for p in entry.split(":", 2)]
        if len(parts) != 3:
            fail(path, entry_no, f"malformed entry line: {entry!r}")
        drive_raw, prg_raw, label = parts

        if not drive_raw.isdigit():
            fail(path, entry_no, f"drive token must be numeric: {drive_raw!r}")
        drive = int(drive_raw, 10)
        if drive < 8 or drive > 11:
            fail(path, entry_no, f"drive must be 8..11: {drive}")

        prg = normalize_prg_token(prg_raw, path, entry_no)

        if not label:
            fail(path, entry_no, "display name is empty")
        if len(label) > 31:
            fail(path, entry_no, f"display name too long ({len(label)} > 31)")

        if i >= len(logical):
            fail(path, entry_no, "missing description line")
        desc_no, desc = logical[i]
        i += 1

        if not desc:
            fail(path, desc_no, "description line is empty")
        if len(desc) > 38:
            fail(path, desc_no, f"description too long ({len(desc)} > 38)")

        out_lines.append(f"{drive}:{prg}:{label}")
        out_lines.append(desc)

    if not out_lines:
        raise ValueError(f"{path}: no catalog entries found")
    if len(out_lines) // 2 > 15:
        raise ValueError(f"{path}: too many entries ({len(out_lines) // 2} > 15)")
    return out_lines


def petscii_lower_byte(ch: str, path: str, line_no: int) -> int:
    code = ord(ch)
    if ch == "\r" or ch == "\n":
        return 13

    if "a" <= ch <= "z":
        return ord(ch.upper())
    if "A" <= ch <= "Z":
        fail(path, line_no, f"unexpected uppercase letter in encoding pass: {ch!r}")
    if 32 <= code <= 126:
        return code

    fail(path, line_no, f"unsupported character U+{code:04X} ({ch!r})")
    return 0


def encode_petscii_lower(lines: List[str], path: str) -> bytes:
    out = bytearray()
    for i, line in enumerate(lines, start=1):
        for ch in line:
            out.append(petscii_lower_byte(ch, path, i))
        out.append(13)
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description="Build PETASCII-lowercase apps.cfg binary")
    ap.add_argument("--input", required=True, help="Source catalog text")
    ap.add_argument("--output", required=True, help="Output binary payload")
    args = ap.parse_args()

    try:
        lines = parse_source(args.input)
        payload = encode_petscii_lower(lines, args.input)
    except ValueError as ex:
        print(f"error: {ex}", file=sys.stderr)
        return 1

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(payload)

    print(f"wrote {args.output} ({len(payload)} bytes, {len(lines)//2} entries)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
