#!/usr/bin/env python3
"""Build deterministic source files for the physical xuzdeflate probe."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import zlib


def pseudo_random(length: int, seed: int) -> bytes:
    value = seed & 0xFFFF
    result = bytearray()
    for _ in range(length):
        value = (value * 25173 + 13849) & 0xFFFF
        result.append(value >> 8)
    return bytes(result)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--run-id", required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "owner.marker").write_bytes(args.run_id.encode("ascii"))

    phrase = b"READYOS-ULTIMATE-ZIP/"
    random_8k = pseudo_random(8192, 0xACE1)
    cases = {
        "EMPTY": b"",
        "REPEAT": (phrase * 1600)[:32768],
        "RANDOM": pseudo_random(4096, 0x4D21),
        # The second 8K half requires the full advertised history distance.
        "CROSS": random_8k + random_8k,
    }
    manifest: dict[str, object] = {"run_id": args.run_id, "cases": {}}
    for name, data in cases.items():
        (args.output / f"{name}.BIN").write_bytes(data)
        manifest["cases"][name] = {
            "size": len(data),
            "crc32": f"{zlib.crc32(data) & 0xFFFFFFFF:08x}",
        }
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
