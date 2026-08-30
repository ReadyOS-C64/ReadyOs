#!/usr/bin/env python3
"""Independent host oracle for a physical Ultimate Zip UI workflow."""

from __future__ import annotations

import argparse
import zipfile
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--fixture-dir", type=Path, required=True)
    parser.add_argument("--results-dir", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--store-archive", type=Path, required=True)
    parser.add_argument("--output-list", type=Path, required=True)
    parser.add_argument("--destination-list", type=Path, required=True)
    parser.add_argument("--destination-tree-list", type=Path, required=True)
    parser.add_argument("--destination-nest-list", type=Path, required=True)
    args = parser.parse_args()

    expected = {
        "LOOSE.BIN": args.fixture_dir / "LOOSE.BIN",
        "TREE/ROOT.TXT": args.fixture_dir / "TREE-ROOT.TXT",
        "TREE/NEST/DEEP.BIN": args.fixture_dir / "TREE-DEEP.BIN",
    }
    local_names = {
        "LOOSE.BIN": "LOOSE.BIN",
        "TREE/ROOT.TXT": "TREE-ROOT.TXT",
        "TREE/NEST/DEEP.BIN": "TREE-DEEP.BIN",
    }
    for member, fixture in expected.items():
        plain = fixture.read_bytes()
        local_name = local_names[member]
        extracted = (args.results_dir / f"extracted-{local_name}").read_bytes()
        readback = (args.results_dir / f"source-{local_name}").read_bytes()
        if extracted != plain or readback != plain:
            raise SystemExit(f"source/extracted byte oracle failed for {member}")
    output_listing = args.output_list.read_text(errors="replace").upper()
    destination_listing = args.destination_list.read_text(errors="replace").upper()
    tree_listing = args.destination_tree_list.read_text(errors="replace").upper()
    nest_listing = args.destination_nest_list.read_text(errors="replace").upper()
    with zipfile.ZipFile(args.archive) as handle:
        infos = handle.infolist()
        names = {item.filename.upper() for item in infos}
        expected_names = {
            "LOOSE.BIN", "TREE/", "TREE/ROOT.TXT",
            "TREE/NEST/", "TREE/NEST/DEEP.BIN",
        }
        if names != expected_names:
            raise SystemExit(
                f"unexpected archive members: {[item.filename for item in infos]!r}"
            )
        for info in infos:
            name = info.filename.upper()
            if name.endswith("/"):
                if info.compress_type != zipfile.ZIP_STORED:
                    raise SystemExit(f"directory method is not Store: {name}")
                continue
            if info.compress_type != zipfile.ZIP_DEFLATED:
                raise SystemExit(f"archive method is not Deflate: {name}")
            if handle.read(info) != expected[name].read_bytes():
                raise SystemExit(f"Python ZIP byte oracle failed for {name}")
    store_expected = {
        "TREE/ROOT.TXT": expected["TREE/ROOT.TXT"],
        "TREE/NEST/DEEP.BIN": expected["TREE/NEST/DEEP.BIN"],
    }
    with zipfile.ZipFile(args.store_archive) as handle:
        infos = handle.infolist()
        names = {item.filename.upper() for item in infos}
        expected_names = {
            "TREE/", "TREE/ROOT.TXT", "TREE/NEST/",
            "TREE/NEST/DEEP.BIN",
        }
        if names != expected_names:
            raise SystemExit(
                f"unexpected current-folder Store members: "
                f"{[item.filename for item in infos]!r}"
            )
        for info in infos:
            name = info.filename.upper()
            if info.compress_type != zipfile.ZIP_STORED:
                raise SystemExit(f"current-folder member is not Store: {name}")
            if not name.endswith("/") and handle.read(info) != store_expected[name].read_bytes():
                raise SystemExit(f"current-folder Store byte oracle failed for {name}")
    if ("ARCHIVE.ZIP" not in output_listing or
            "STORE.ZIP" not in output_listing or
            "CANCEL.ZIP" in output_listing or
            "LOOSE.BIN" not in destination_listing or
            "TREE" not in destination_listing or
            "ROOT.TXT" not in tree_listing or "NEST" not in tree_listing or
            "DEEP.BIN" not in nest_listing):
        raise SystemExit("final directory listing oracle failed")
    if any(".UZTMP" in listing for listing in (
            output_listing, destination_listing, tree_listing, nest_listing)):
        raise SystemExit("temporary uZIP file remains")
    print("current-folder Store, safe cancel, recursive Deflate/Extract, and Python ZIP oracle passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
