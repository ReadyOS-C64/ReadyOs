#!/usr/bin/env python3
"""Generate one exact owned root for a physical xuzstore run."""

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
        raise SystemExit("run id must be 8-48 uppercase letters/digits/hyphens")
    root = f"/{args.volume}/READYOS_UZIP_TEST/{args.run_id}"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "#ifndef XUZSTORE_CONFIG_H\n#define XUZSTORE_CONFIG_H\n\n"
        f'#define XUZSTORE_OWNED_ROOT "{root.lower()}"\n'
        f'#define XUZSTORE_OWNER_TEXT "{args.run_id.lower()}"\n\n'
        "#endif\n",
        encoding="ascii",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
