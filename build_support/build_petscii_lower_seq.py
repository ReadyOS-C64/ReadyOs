#!/usr/bin/env python3
"""
Build a plain-text SEQ payload using lowercase PETASCII semantics.

Input source is human-editable UTF-8 text. Alphabetic characters must be
lowercase in source. Output bytes encode a-z as ASCII A-Z byte values, which
is the standard lowercase PETASCII convention for C64 text flows.
"""

from __future__ import annotations

import argparse
import os
import sys


def has_upper_ascii(text: str) -> bool:
    for ch in text:
        if "A" <= ch <= "Z":
            return True
    return False


def petscii_lower_byte(ch: str, path: str, line_no: int) -> int:
    code = ord(ch)
    if "a" <= ch <= "z":
        return ord(ch.upper())
    if "A" <= ch <= "Z":
        raise ValueError(f"{path}:{line_no}: uppercase letters are not allowed: {ch!r}")
    if 32 <= code <= 126:
        return code
    raise ValueError(f"{path}:{line_no}: unsupported character U+{code:04X} ({ch!r})")


def encode_lines(path: str) -> bytes:
    out = bytearray()
    with open(path, "r", encoding="utf-8", errors="strict") as f:
        for line_no, raw in enumerate(f.read().splitlines(), start=1):
            if has_upper_ascii(raw):
                raise ValueError(f"{path}:{line_no}: alphabetic text must be lowercase")
            for ch in raw:
                out.append(petscii_lower_byte(ch, path, line_no))
            out.append(13)
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description="Build lowercase PETASCII SEQ payload")
    ap.add_argument("--input", required=True, help="UTF-8 source text")
    ap.add_argument("--output", required=True, help="Output SEQ payload")
    args = ap.parse_args()

    try:
        payload = encode_lines(args.input)
    except ValueError as ex:
        print(f"error: {ex}", file=sys.stderr)
        return 1

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(payload)

    print(f"wrote {args.output} ({len(payload)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
