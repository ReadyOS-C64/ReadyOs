#!/usr/bin/env python3
"""Generate exact fixture constants for one physical xuzinflate build."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


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
    lines = [
        "#ifndef XUZINFLATE_CONFIG_H",
        "#define XUZINFLATE_CONFIG_H",
        "",
        f'#define XUZINFLATE_OWNED_ROOT "{root}"',
        f'#define XUZINFLATE_OWNER_TEXT "{args.run_id.lower()}"',
        "",
    ]
    for name in ("EMPTY", "STORED", "FIXED", "DYNAMIC"):
        item = manifest["positive"][name]
        crc = int(item["crc32"], 16)
        lines.extend([
            f"#define XUZ_{name}_PACKED {item['packed_size']}u",
            f"#define XUZ_{name}_SIZE {item['size']}u",
            f"#define XUZ_{name}_CRC0 0x{crc & 0xff:02X}u",
            f"#define XUZ_{name}_CRC1 0x{(crc >> 8) & 0xff:02X}u",
            f"#define XUZ_{name}_CRC2 0x{(crc >> 16) & 0xff:02X}u",
            f"#define XUZ_{name}_CRC3 0x{(crc >> 24) & 0xff:02X}u",
            "",
        ])
    for name in ("TRUNC", "TRAIL", "BADTYPE", "BADSTORED", "BADDIST",
                 "BADLENGTH", "BADRSVDIST", "BADREPEAT", "BADTREE"):
        item = manifest["negative"][name]
        lines.append(f"#define XUZ_{name}_PACKED {item['packed_size']}u")
    lines.append("")
    lines.extend(["#endif", ""])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
