#!/usr/bin/env python3
"""Generate the compile-time owned root for one physical xuzio run."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--volume", choices=("USB1", "SD"), default="USB1")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    if not re.fullmatch(r"[A-Z0-9][A-Z0-9-]{7,47}", args.run_id):
        raise SystemExit("run id must be 8-48 uppercase ASCII letters/digits/hyphens")
    root = f"/{args.volume}/READYOS_UZIP_TEST/{args.run_id}"
    # Lowercase source gives ASCII-range uppercase letters under cc65. The DOS
    # boundary additionally normalizes PETSCII `_` ($A4) to ASCII $5F.
    c_root = root.lower()
    c_owner = args.run_id.lower()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "#ifndef XUZIO_CONFIG_H\n"
        "#define XUZIO_CONFIG_H\n\n"
        f'#define XUZIO_OWNED_ROOT "{c_root}"\n'
        f'#define XUZIO_OWNER_TEXT "{c_owner}"\n\n'
        "#endif\n",
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
