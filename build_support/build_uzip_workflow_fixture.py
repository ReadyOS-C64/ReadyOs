#!/usr/bin/env python3
"""Build the deterministic physical ReadyOS uZIP workflow fixture."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--run-id", required=True)
    args = parser.parse_args()

    loose = args.output / "LOOSE.BIN"
    tree_root = args.output / "TREE-ROOT.TXT"
    tree_deep = args.output / "TREE-DEEP.BIN"
    owner = args.output / "owner.marker"
    args.output.mkdir(parents=True, exist_ok=True)
    loose_payload = bytes(
        ((index * 17) ^ (index >> 2) ^ (index // 97)) & 0xFF
        for index in range(1536)
    )
    root_payload = (
        b"ReadyOS Ultimate ZIP recursive workflow\r\n" * 19
        + args.run_id.encode("ascii") + b"\r\n"
    )
    deep_payload = bytes(
        ((index * 29) ^ (index >> 1) ^ 0xA5) & 0xFF
        for index in range(2305)
    )
    loose.write_bytes(loose_payload)
    tree_root.write_bytes(root_payload)
    tree_deep.write_bytes(deep_payload)
    owner.write_text(f"READYOS-UZIP-WORKFLOW\n{args.run_id}\n", encoding="ascii")
    manifest = {
        "run_id": args.run_id,
        "members": {
            "LOOSE.BIN": hashlib.sha256(loose_payload).hexdigest(),
            "TREE/ROOT.TXT": hashlib.sha256(root_payload).hexdigest(),
            "TREE/NEST/DEEP.BIN": hashlib.sha256(deep_payload).hexdigest(),
        },
        "archive_name": "archive.zip",
    }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
