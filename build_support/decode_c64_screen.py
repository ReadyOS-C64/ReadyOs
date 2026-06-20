#!/usr/bin/env python3
"""Decode a 40x25 C64 screen-code dump into readable text."""

from __future__ import annotations

import argparse
from pathlib import Path


def decode_screen_code(value: int) -> str:
    if value == 0x20:
        return " "
    if 0x01 <= value <= 0x1A:
        return chr(ord("A") + value - 1)
    if 0x30 <= value <= 0x39:
        return chr(value)
    if 0x20 <= value <= 0x3F:
        table = {
            0x21: "!",
            0x22: '"',
            0x23: "#",
            0x24: "$",
            0x25: "%",
            0x26: "&",
            0x27: "'",
            0x28: "(",
            0x29: ")",
            0x2A: "*",
            0x2B: "+",
            0x2C: ",",
            0x2D: "-",
            0x2E: ".",
            0x2F: "/",
            0x3A: ":",
            0x3B: ";",
            0x3C: "<",
            0x3D: "=",
            0x3E: ">",
            0x3F: "?",
        }
        return table.get(value, chr(value))
    if 0x41 <= value <= 0x5A:
        return chr(ord("a") + value - 0x41)
    return "."


def decode_screen(data: bytes, width: int = 40, height: int = 25) -> list[str]:
    padded = data[: width * height].ljust(width * height, b" ")
    lines = []
    for row in range(height):
        start = row * width
        text = "".join(decode_screen_code(b) for b in padded[start : start + width])
        lines.append(text.rstrip())
    return lines


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("screen_bin", type=Path)
    parser.add_argument("--output", "-o", type=Path)
    args = parser.parse_args()

    lines = decode_screen(args.screen_bin.read_bytes())
    text = "\n".join(f"{idx:02d}: {line}" for idx, line in enumerate(lines)) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
