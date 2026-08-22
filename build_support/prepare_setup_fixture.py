#!/usr/bin/env python3
"""Create a disposable Ultimate SETUP D81 fixture with a chosen saved path."""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import tempfile


def run(args: list[str]) -> None:
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--saved-path", required=True)
    parser.add_argument("--bootstrap-output", type=pathlib.Path)
    parser.add_argument("--setup-prg", type=pathlib.Path)
    args = parser.parse_args()
    if args.saved_path and (
        not args.saved_path.startswith("/")
        or not args.saved_path.lower().endswith(".d81")
    ):
        parser.error("--saved-path must be empty or an absolute D81 path")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(args.source, args.output)
    with tempfile.TemporaryDirectory(prefix="readyos-setup-fixture-") as raw_tmp:
        temp = pathlib.Path(raw_tmp)
        config_path = temp / "apps.cfg"
        run(["c1541", str(args.output), "-read", "apps.cfg,s", str(config_path)])
        data = config_path.read_bytes()
        lines = data.splitlines(keepends=True)
        found_dma = found_path = False
        for index, line in enumerate(lines):
            upper = line.upper()
            ending = b"\r" if line.endswith(b"\r") else (b"\n" if line.endswith(b"\n") else b"")
            if upper.startswith(b"DMA_LOADING="):
                lines[index] = b"DMA_LOADING=1" + ending
                found_dma = True
            elif upper.startswith(b"C64U_IMAGE_PATH="):
                lines[index] = b"C64U_IMAGE_PATH=" + args.saved_path.encode("ascii") + ending
                found_path = True
        if not (found_dma and found_path):
            raise SystemExit("fixture apps.cfg is missing DMA/path keys")
        config_path.write_bytes(b"".join(lines))
        run(["c1541", str(args.output), "-delete", "apps.cfg"])
        run(["c1541", str(args.output), "-write", str(config_path), "apps.cfg,s"])
        if args.bootstrap_output is not None:
            if args.setup_prg is None or not args.setup_prg.is_file():
                parser.error("--bootstrap-output requires an existing --setup-prg")
            args.bootstrap_output.parent.mkdir(parents=True, exist_ok=True)
            run(["c1541", "-format", "readysetup,rs", "d81",
                 str(args.bootstrap_output)])
            # SETUP first keeps the hardware harness's proven LOAD"*" boot
            # deterministic. This disposable image is automation-only.
            run(["c1541", str(args.bootstrap_output), "-write",
                 str(args.setup_prg), "setup"])
            run(["c1541", str(args.bootstrap_output), "-write",
                 str(config_path), "apps.cfg,s"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
