#!/usr/bin/env python3
"""
Temporary named EasyFlash variant runner.

Builds the EasyFlash flavor with launcher-only CPP overrides, then runs VICE
headlessly and captures an exit screenshot for quick comparison runs.
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(cmd: list[str], *, env: dict[str, str] | None = None, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=str(ROOT), env=env, check=check, text=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--screenshot", required=True)
    ap.add_argument("--limitcycles", type=int, default=20000000)
    ap.add_argument("--vice", default=os.environ.get("VICE", "x64sc"))
    ap.add_argument("--probe", type=int, default=3)
    ap.add_argument("--entry-limit", type=int, default=16)
    ap.add_argument("--interactive", type=int, choices=(0, 1), default=0)
    ap.add_argument("--autolaunch-index", type=int, default=0)
    ap.add_argument("--set-shim-drive", type=int, choices=(0, 1), default=1)
    ap.add_argument("--sync-bitmap", type=int, choices=(0, 1), default=0)
    ap.add_argument("--seed-hotkeys", type=int, choices=(0, 1), default=0)
    args = ap.parse_args()

    cppflags = " ".join(
        [
            f"-DREADYOS_LAUNCHER_VARIANT_EASYFLASH_VISUAL_PROBE={args.probe}",
            f"-DREADYOS_LAUNCHER_VARIANT_EASYFLASH_PROBE_ENTRY_LIMIT={args.entry_limit}",
            f"-DREADYOS_LAUNCHER_VARIANT_EASYFLASH_PROBE_INTERACTIVE={args.interactive}",
            f"-DREADYOS_LAUNCHER_VARIANT_EASYFLASH_PROBE_AUTOLAUNCH_INDEX={args.autolaunch_index}",
            f"-DREADYOS_LAUNCHER_VARIANT_EASYFLASH_PROBE_SET_SHIM_DRIVE={args.set_shim_drive}",
            f"-DREADYOS_LAUNCHER_VARIANT_EASYFLASH_PROBE_SYNC_BITMAP={args.sync_bitmap}",
            f"-DREADYOS_LAUNCHER_VARIANT_EASYFLASH_PROBE_SEED_HOTKEYS={args.seed_hotkeys}",
        ]
    )

    env = os.environ.copy()
    env["EASYFLASH_LAUNCHER_CPPFLAGS"] = cppflags

    screenshot = Path(args.screenshot)
    screenshot.parent.mkdir(parents=True, exist_ok=True)
    if screenshot.exists():
        screenshot.unlink()

    run(["make", "easyflash"], env=env)
    vice_result = run(
        [
            args.vice,
            "-default",
            "+sound",
            "-warp",
            "-reu",
            "-reusize",
            "16384",
            "-cartcrt",
            "releases/0.2/precog-easyflash/readyos_easyflash.crt",
            "-drive8type",
            "1541",
            "-devicebackend8",
            "0",
            "+busdevice8",
            "-8",
            "releases/0.2/precog-easyflash/readyos_data.d64",
            "-limitcycles",
            str(args.limitcycles),
            "-exitscreenshot",
            str(screenshot),
        ],
        env=env,
        check=False,
    )

    if not screenshot.exists():
        print(f"error: missing screenshot: {screenshot}", file=sys.stderr)
        return 1

    print(f"screenshot: {screenshot}")
    print(f"cppflags: {cppflags}")
    print(f"vice_rc: {vice_result.returncode}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
