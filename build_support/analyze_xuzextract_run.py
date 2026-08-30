#!/usr/bin/env python3
"""Strict host oracle for the physical xuzextract transaction probe."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import zlib


def u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 2], "little")


def u32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "little")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


def run_dir_from_log(path: pathlib.Path) -> pathlib.Path:
    source = path.read_text(encoding="utf-8-sig", errors="ignore")
    matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', source)
    if not matches:
        raise SystemExit(f"{path}: no physical-run manifest path found")
    return pathlib.Path(matches[-1]).expanduser().resolve().parent


def find_dump(run_dir: pathlib.Path) -> pathlib.Path:
    matches = [path for path in run_dir.rglob("*xuzextract_result_033c*.bin")
               if path.is_file()]
    if not matches:
        raise SystemExit(f"no xuzextract result dump below {run_dir}")
    return max(matches, key=lambda path: path.stat().st_mtime)


def listing_names(path: pathlib.Path) -> set[str]:
    return {line.strip().rstrip("/").upper()
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines()
            if line.strip()}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("run_log", type=pathlib.Path)
    parser.add_argument("fixtures", type=pathlib.Path)
    parser.add_argument("downloads", type=pathlib.Path)
    parser.add_argument("--result-only", action="store_true")
    parser.add_argument("--json-output", type=pathlib.Path)
    args = parser.parse_args()

    manifest = json.loads((args.fixtures / "manifest.json").read_text())
    owner = (args.fixtures / "owner.marker").read_bytes()
    run_dir = run_dir_from_log(args.run_log)
    dump = find_dump(run_dir)
    raw = dump.read_bytes()
    at = raw.find(b"XZE1")
    require(at >= 0 and len(raw) - at >= 128,
            f"{dump}: complete XZE1 result is missing")
    result = raw[at:at + 128]
    package_path = args.fixtures.parent / "uzpack-production.prg"
    package_file = package_path.read_bytes()
    require(package_file[:2] == b"\x00\x00" and
            package_file[2:6] == b"UZPK",
            f"{package_path}: canonical PRG/package header is missing")
    package = package_file[2:]
    reader_descriptor = 12 + 5 * 8
    reader_offset = u16(package, reader_descriptor)
    reader_head = package[reader_offset:reader_offset + 2]

    require(result[4:8] == bytes((1, 1, 9, 0)),
            f"C64 extraction failed: version={result[4]} done={result[5]} "
            f"stage={result[6]} failure=${result[7]:02X} "
            f"good_reader={result[60]} bad_reader={result[61]} "
            f"conflict=({result[70]},{result[71]},{result[72]}) "
            f"badcrc=({result[73]},{result[74]},{result[75]}) "
            f"owner=(stage={result[90]},got={result[91]},flags={result[92]},"
            f"status={result[93]},mismatch={result[94]},data={result[95]},"
            f"head={result[96:104].hex()}) "
            f"good_pf=(stage={result[104]},flags={result[105]},"
            f"status={result[106]},data={result[107]},open={result[108]},"
            f"size={u32(result, 28)}) "
            f"bad_pf=(stage={result[110]},flags={result[111]},"
            f"status={result[112]},data={result[113]},open={result[114]},"
            f"size={u32(result, 40)}) "
            f"pf_stat=(len={result[115]},head={result[116:124].hex()}) "
            f"handoff=(stage={result[13]},"
            f"source={result[82:84].hex()},ram={result[124:126].hex()},"
            f"offset=${u16(result, 126):04X})")
    require(result[8] != 0xFF and result[9] != 0xFF and result[10] != 0xFF and
            len({result[8], result[9], result[10]}) == 3,
            "package/work/catalog banks are invalid or aliased")
    require(result[11:13] == b"\x01\x01", "work/catalog banks were not released")
    require(u16(result, 16) in (0xDF1C, 0xDE1C, 0xDFFC), "invalid UCI base")
    require(result[52:60] == bytes((1, 1, 2, 1, 1, 1, 1, 1)),
            "preflight/extract/conflict/CRC/CPU evidence is incomplete")
    require(result[60:64] == bytes((0, 0, 7, 6)),
            "reader errors or uZPK descriptor differ")
    require(result[64:68] == zlib.crc32(owner).to_bytes(4, "little"),
            "fixture cookie differs")
    require(result[90:92] == bytes((7, len(owner))) and result[94] == 0xFF and
            result[95] == len(owner) and result[96:104] == owner[:8],
            "C64 owner-marker validation evidence differs")
    require(result[104] == 7 and result[110] == 7,
            "preflight substage or transport evidence differs")
    require(result[13] == 5 and u16(result, 126) == reader_offset and
            result[82:84] == reader_head and
            result[124:126] == reader_head,
            "compact reader source/RAM handoff evidence differs")
    require(result[68:76] == bytes((4, 4, 8, 0, 0, 6, 6, 0)),
            "success counts or cleanup error classifications differ")
    require(result[62:64] == bytes((7, 6)),
            "compact uZPK v7 package descriptor was not exercised")
    require(result[76:78] == bytes((len(manifest["good_entries"]), 1)),
            "preflight entry counts differ")
    require(result[78] <= 96 and result[79] <= 64 and result[80] <= 160 and
            result[81] <= 300, "fixed C64 state windows are too small")
    methods = bytes(entry["method"] for entry in manifest["good_entries"])
    require(result[84:89] == methods, "parsed good-entry methods differ")
    directory_mask = sum(1 << index for index, entry in
                         enumerate(manifest["good_entries"])
                         if entry["directory"])
    require(result[89] == directory_mask, "parsed directory mask differs")

    require(u32(result, 28) == manifest["good_archive_size"] and
            u32(result, 32) == manifest["good_central_offset"] and
            u32(result, 36) == manifest["good_central_size"],
            "good archive geometry differs")
    require(u32(result, 40) == manifest["bad_archive_size"] and
            u32(result, 44) == manifest["bad_central_offset"] and
            u32(result, 48) == manifest["bad_central_size"],
            "bad-CRC archive geometry differs")
    calls = u16(result, 20)
    callback_bytes = u32(result, 22)
    max_request = u16(result, 26)
    require(calls >= 20 and callback_bytes >=
            manifest["good_central_size"] + manifest["bad_central_size"] and
            1 <= max_request <= 512,
            "bounded random-access preflight evidence is incomplete")

    base_report = {
        "physical_result": "pass",
        "physical_run_dir": str(run_dir),
        "dump": str(dump),
        "uci_base": f"{u16(result, 16):04x}",
        "banks": {"package": result[8], "work": result[9],
                  "catalog": result[10]},
        "banks_released": "pass",
        "preflight": {"archives": 2, "entries": result[76] + result[77],
                      "callback_calls": calls,
                      "callback_bytes": callback_bytes,
                      "max_request": max_request},
        "package_version": result[62],
        "reader_handoff": {"offset": reader_offset,
                           "source_head": result[82:84].hex(),
                           "ram_head": result[124:126].hex()},
        "state_sizes": {"dos": result[78], "reader": result[79],
                        "record": result[80], "extract": result[81]},
    }
    if args.result_only:
        rendered = json.dumps(base_report, indent=2, sort_keys=True)
        print(rendered)
        if args.json_output:
            args.json_output.write_text(rendered + "\n", encoding="utf-8")
        return 0

    require((args.downloads / "STORE.BIN").read_bytes() ==
            (args.fixtures / "expected-store.bin").read_bytes(),
            "physical Store output differs byte-for-byte")
    require((args.downloads / "DEFLATE.BIN").read_bytes() ==
            (args.fixtures / "expected-deflate.bin").read_bytes(),
            "physical Deflate output differs byte-for-byte")
    require((args.downloads / "KEEP.BIN").read_bytes() ==
            (args.fixtures / "existing.keep").read_bytes(),
            "existing destination sentinel was changed")

    root = listing_names(args.downloads / "out.list")
    nest = listing_names(args.downloads / "nest.list")
    deep = listing_names(args.downloads / "deep.list")
    existing = listing_names(args.downloads / "existing.list")
    bad = listing_names(args.downloads / "bad.list")
    require({"NEST", "EXISTING", "BAD"}.issubset(root),
            f"destination root listing is incomplete: {sorted(root)!r}")
    require({"DEEP", "STORE.BIN"}.issubset(nest),
            f"nested listing is incomplete: {sorted(nest)!r}")
    require("DEFLATE.BIN" in deep and existing == {"KEEP.BIN"},
            "deep output or existing sentinel listing differs")
    require("CRC.BIN" not in bad, "bad-CRC final file became visible")
    all_names = root | nest | deep | existing | bad
    require(not any(name.startswith(".UZTMP") for name in all_names),
            f"temporary extraction file leaked: {sorted(all_names)!r}")

    report = {
        **base_report,
        "store_bytes": (args.downloads / "STORE.BIN").stat().st_size,
        "deflate_bytes": (args.downloads / "DEFLATE.BIN").stat().st_size,
        "nested_paths": "pass",
        "existing_destination_preserved": "pass",
        "bad_crc_rejected": "pass",
        "temporary_cleanup": "pass",
    }
    rendered = json.dumps(report, indent=2, sort_keys=True)
    print(rendered)
    if args.json_output:
        args.json_output.write_text(rendered + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
