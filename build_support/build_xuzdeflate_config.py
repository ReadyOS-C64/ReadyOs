#!/usr/bin/env python3
"""Generate exact fixture constants for one physical xuzdeflate build."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import zlib


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--volume", choices=("USB1", "SD"), default="USB1")
    parser.add_argument("--fixture-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Z0-9][A-Z0-9-]{7,43}", args.run_id):
        raise SystemExit("run id must be 8-44 uppercase letters/digits/hyphens")
    manifest = json.loads((args.fixture_dir / "manifest.json").read_text())
    root = f"/{args.volume}/READYOS_UZIP_TEST/{args.run_id}".lower()
    cookie = zlib.crc32(args.run_id.encode("ascii")).to_bytes(4, "little")
    lines = [
        "#ifndef XUZDEFLATE_CONFIG_H",
        "#define XUZDEFLATE_CONFIG_H",
        "",
        f'#define XUZDEFLATE_OWNED_ROOT "{root}"',
        f'#define XUZDEFLATE_OWNER_TEXT "{args.run_id.lower()}"',
        f"#define XUZDEFLATE_OWNER_LENGTH {len(args.run_id)}u",
        f"#define XUZDEFLATE_COOKIE0 0x{cookie[0]:02X}u",
        f"#define XUZDEFLATE_COOKIE1 0x{cookie[1]:02X}u",
        f"#define XUZDEFLATE_COOKIE2 0x{cookie[2]:02X}u",
        f"#define XUZDEFLATE_COOKIE3 0x{cookie[3]:02X}u",
        "",
    ]
    for name in ("EMPTY", "REPEAT", "RANDOM", "CROSS"):
        item = manifest["cases"][name]
        crc = int(item["crc32"], 16)
        lines.extend([
            f"#define XUZD_{name}_SIZE {item['size']}u",
            f"#define XUZD_{name}_CRC0 0x{crc & 0xff:02X}u",
            f"#define XUZD_{name}_CRC1 0x{(crc >> 8) & 0xff:02X}u",
            f"#define XUZD_{name}_CRC2 0x{(crc >> 16) & 0xff:02X}u",
            f"#define XUZD_{name}_CRC3 0x{(crc >> 24) & 0xff:02X}u",
            "",
        ])
    lines.extend(["#endif", ""])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
