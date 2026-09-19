#!/usr/bin/env python3
"""Confirm READY from video pixels only; never contacts the C64 or reads RAM.

Patterns are 48x8 binary masks of visually verified C64 READY. glyphs supplied
as JSON (upper/lower). A match must be the final text on screen, followed only
by an optional cursor on the next row. Require two distinct captured frames.
Unrecognized screens remain pending for manual visual inspection.
"""
import argparse
import json
from pathlib import Path
import time

from PIL import Image


def ready_position(path, patterns, allowed):
    with Image.open(path) as source:
        image = source.convert('RGB')
    if image.size != (384, 272):
        return None
    bg = image.getpixel((351, 230))
    pixel = image.load()
    for y in range(35, 228, 8):
        actual = [''.join('#' if pixel[x, row] != bg else '.' for x in range(32, 80))
                  for row in range(y, y + 8)]
        for name in allowed:
            if actual != patterns[name]:
                continue
            if any(pixel[x, row] != bg for row in range(y, y + 8) for x in range(80, 352)):
                continue
            # No typed command or continuing output may follow READY.
            if any(pixel[x, row] != bg for row in range(y + 8, 235) for x in range(32, 352)
                   if not (row < y + 16 and x < 40)):
                continue
            cursor = [pixel[x, row] != bg for row in range(y + 8, min(y + 16, 235))
                      for x in range(32, 40)]
            if any(cursor) and not all(cursor):
                continue  # A typed glyph below READY is not the block cursor.
            return name, y
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('patterns', type=Path)
    args = parser.parse_args()
    patterns = json.loads(args.patterns.read_text())
    previous = None
    while not (args.run / 'complete').exists():
        pending_path = args.run / 'pending.json'
        try:
            pending = json.loads(pending_path.read_text())
            path = Path(pending['image'])
            allowed = ['upper'] if pending['label'].startswith(('stock_prompt', 'preboot_load')) else ['lower']
            position = ready_position(path, patterns, allowed)
            stamp = path.stat().st_mtime_ns
            current = (str(path), position, stamp)
            if position and previous and previous[:2] == current[:2] and previous[2] != stamp:
                Path(pending['acknowledge']).write_text(
                    f'Video pixels: final {position[0]} READY. at y={position[1]}, '
                    'no following text, matched in two distinct frames; no RAM reads.\n')
                print('VIDEO READY', pending['suite'], pending['label'], flush=True)
                previous = None
            else:
                previous = current if position else None
        except (FileNotFoundError, json.JSONDecodeError, OSError):
            previous = None
        time.sleep(1)


if __name__ == '__main__':
    main()
