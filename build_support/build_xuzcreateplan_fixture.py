#!/usr/bin/env python3
"""Build an owned recursive tree and cc65 expectations for xuzcreateplan."""

from __future__ import annotations

import argparse
import json
import re
import zlib
from pathlib import Path


def c_source_name(name: str) -> str:
    if not re.fullmatch(r"[A-Z0-9_./-]+", name):
        raise ValueError(f"unsupported fixture name {name!r}")
    return name.lower()


def payload(name: str, size: int) -> bytes:
    seed = name.encode("ascii") + b"\r\n"
    return (seed * ((size + len(seed) - 1) // len(seed)))[:size]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--header", type=Path, required=True)
    parser.add_argument("--volume", choices=("USB1", "SD"), default="USB1")
    args = parser.parse_args()
    if not re.fullmatch(r"XUZCREATEPLAN-[A-Z0-9-]{8,40}", args.run_id):
        raise SystemExit("run id must be an owned XUZCREATEPLAN-* identifier")

    files: dict[str, bytes] = {
        "LOOSE.PRG": payload("LOOSE.PRG", 257),
        "TOP/A.BIN": payload("TOP/A.BIN", 513),
        "TOP/SUB/B.BIN": payload("TOP/SUB/B.BIN", 1025),
        "TOP/SUB/DEEP/C.BIN": payload("TOP/SUB/DEEP/C.BIN", 33),
    }
    for index in range(15):
        name = f"TOP/F{index:02d}.DAT"
        files[name] = payload(name, index + 1)
    directories = ["TOP/", "TOP/SUB/", "TOP/EMPTY/", "TOP/SUB/DEEP/"]
    entries = [
        ("LOOSE.PRG", False),
        ("TOP/", True),
        ("TOP/A.BIN", False),
        *((f"TOP/F{index:02d}.DAT", False) for index in range(15)),
        ("TOP/SUB/", True),
        ("TOP/EMPTY/", True),
        ("TOP/SUB/B.BIN", False),
        ("TOP/SUB/DEEP/", True),
        ("TOP/SUB/DEEP/C.BIN", False),
    ]
    if len(entries) != 23:
        raise RuntimeError("fixture entry count changed")

    args.output.mkdir(parents=True, exist_ok=True)
    tree = args.output / "SOURCE"
    for directory in directories:
        (tree / directory).mkdir(parents=True, exist_ok=True)
    for name, data in files.items():
        path = tree / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    owner = args.run_id.encode("ascii")
    (args.output / "owner.marker").write_bytes(owner)

    root = f"/{args.volume}/READYOS_UZIP_TEST/{args.run_id}".lower()
    lines = [
        "#ifndef XUZCREATEPLAN_CONFIG_H",
        "#define XUZCREATEPLAN_CONFIG_H",
        "",
        f'#define XUZCREATEPLAN_OWNED_ROOT "{root}"',
        f'#define XUZCREATEPLAN_OWNER_TEXT "{args.run_id.lower()}"',
        f"#define XUZCREATEPLAN_OWNER_LENGTH {len(owner)}u",
        f'#define XUZCREATEPLAN_SOURCE_BASE "{root}/source"',
        f'#define XUZCREATEPLAN_OUTPUT_PATH "{root}/result.zip"',
        f"#define XUZCREATEPLAN_ENTRY_COUNT {len(entries)}u",
        f"#define XUZCREATEPLAN_FILE_COUNT {len(files)}u",
        f"#define XUZCREATEPLAN_DIRECTORY_COUNT {len(directories)}u",
        "#define XUZCREATEPLAN_LIST_CALLS 5u",
    ]
    for index, (name, directory) in enumerate(entries):
        lines.extend([
            f'#define XUZCREATEPLAN_NAME_{index} "{c_source_name(name)}"',
            f"#define XUZCREATEPLAN_DIRECTORY_{index} {1 if directory else 0}u",
        ])
    cookie = zlib.crc32(owner).to_bytes(4, "little")
    lines.extend(f"#define XUZCREATEPLAN_COOKIE_{i} 0x{byte:02X}u"
                 for i, byte in enumerate(cookie))
    lines.extend(["", "#endif", ""])
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text("\n".join(lines), encoding="ascii")

    manifest = {
        "run_id": args.run_id,
        "root": root.upper(),
        "source_base": f"{root}/source".upper(),
        "output_path": f"{root}/result.zip".upper(),
        "entry_count": len(entries),
        "file_count": len(files),
        "directory_count": len(directories),
        "list_calls": 5,
        "entries": [
            {"name": name, "directory": directory,
             "method": 0 if directory else 8}
            for name, directory in entries
        ],
        "files": {name: len(data) for name, data in files.items()},
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
