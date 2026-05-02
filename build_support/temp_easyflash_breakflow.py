#!/usr/bin/env python3
"""
Temporary EasyFlash monitor-script runner.

Writes a deterministic VICE monitor script with early breakpoints, runs a
headless CRT boot, and saves the monitor log for repeated launcher debugging.
"""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def write_monitor(path: Path, break1: int, break2: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "\n".join(
            [
                f"break ${break1:04x}",
                "x",
                "r",
                "m $0100 $01ff",
                "m $0400 $047f",
                "m $1000 $103f",
                "m $2ff0 $3020",
                "m $4e80 $4ec0",
                "m $c7a0 $c7df",
                "m $c820 $c83f",
                f"break ${break2:04x}",
                "x",
                "r",
                "m $0400 $047f",
                "m $1000 $103f",
                "m $2ff0 $3020",
                "m $4e80 $4ec0",
                "m $c7a0 $c7df",
                "m $c820 $c83f",
                "q",
                "",
            ]
        ),
        encoding="ascii",
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--vice", default="x64sc")
    ap.add_argument("--crt", required=True)
    ap.add_argument("--d64", required=True)
    ap.add_argument("--mon", required=True)
    ap.add_argument("--log", required=True)
    ap.add_argument("--break1", type=lambda s: int(s, 0), default=0x1000)
    ap.add_argument("--break2", type=lambda s: int(s, 0), default=0x4E98)
    ap.add_argument("--limitcycles", type=int, default=30000000)
    args = ap.parse_args()

    mon = Path(args.mon)
    log = Path(args.log)
    write_monitor(mon, args.break1, args.break2)
    log.parent.mkdir(parents=True, exist_ok=True)

    with log.open("w", encoding="utf-8", errors="replace") as out:
        proc = subprocess.run(
            [
                args.vice,
                "-console",
                "-default",
                "+sound",
                "-warp",
                "-reu",
                "-reusize",
                "16384",
                "-cartcrt",
                args.crt,
                "-drive8type",
                "1541",
                "-devicebackend8",
                "0",
                "+busdevice8",
                "-8",
                args.d64,
                "-moncommands",
                str(mon),
                "-limitcycles",
                str(args.limitcycles),
            ],
            cwd=str(ROOT),
            stdout=out,
            stderr=subprocess.STDOUT,
            text=True,
        )
    return proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())
