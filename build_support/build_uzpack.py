#!/usr/bin/env python3
"""Build the compact launcher-preloaded uZPK v7 image from linked phases."""

from __future__ import annotations

import argparse
import binascii
import re
import zlib
from pathlib import Path


RAW_PACKAGE_SIZE = 0x7000
PACKAGE_MAX_SIZE = 0x5F00
PACKAGE_HEADER_SIZE = 64
PACKAGE_VERSION = 7
PHASE_COUNT = 6
DESCRIPTOR_BASE = 12
DESCRIPTOR_SIZE = 8
CREATE_RAW_OFFSET = 0x5F00
CREATE_MAGIC = b"UZCR"
CREATE_VERSION = 2
CREATE_HEADER_SIZE = 28
PLAN_RUN = 0x9000
PLAN_MAX_SIZE = 0x2000
UI_MAGIC = b"UZUI"
UI_VERSION = 1
UI_HEADER_SIZE = 20
# $D000-$EFFF caches the modal planner and $F000-$FFFF caches the unchanged
# Create coordinator after cold expansion. The on-disk resource must end
# before either cache window.
RESOURCE_MAX_SIZE = 0xD000


def symbol(map_text: str, name: str) -> int:
    match = re.search(
        rf"(?:^|\s){re.escape(name)}\s+([0-9A-Fa-f]{{6}})(?:\s|$)",
        map_text,
        re.MULTILINE,
    )
    if not match:
        raise SystemExit(f"missing map symbol {name}")
    return int(match.group(1), 16)


def optional_symbol(map_text: str, name: str) -> int | None:
    match = re.search(
        rf"(?:^|\s){re.escape(name)}\s+([0-9A-Fa-f]{{6}})(?:\s|$)",
        map_text,
        re.MULTILINE,
    )
    return int(match.group(1), 16) if match else None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", type=Path, required=True)
    parser.add_argument("--map", dest="map_path", type=Path, required=True)
    parser.add_argument("--plan-raw", type=Path)
    parser.add_argument("--ui-raw", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    image = args.raw.read_bytes()
    if len(image) != RAW_PACKAGE_SIZE:
        raise SystemExit(
            f"raw package size is {len(image)}, expected {RAW_PACKAGE_SIZE}"
        )
    map_text = args.map_path.read_text(encoding="utf-8", errors="replace")

    phases = (
        ("JOB_CODE", 0x0100, "__JOB_CODE_LOAD__", "__JOB_CODE_RUN__",
         symbol(map_text, "__JOB_RODATA_LOAD__") +
         symbol(map_text, "__JOB_RODATA_SIZE__") -
         symbol(map_text, "__JOB_CODE_LOAD__")),
        ("INFLATE_CODE", 0x1200, "__INFLATE_CODE_LOAD__",
         "__INFLATE_CODE_RUN__",
         symbol(map_text, "__INFLATE_RODATA_LOAD__") +
         symbol(map_text, "__INFLATE_RODATA_SIZE__") -
         symbol(map_text, "__INFLATE_CODE_LOAD__")),
        ("DEFLATE_MATCH_CODE", 0x2200, "__DEFLATE_MATCH_CODE_LOAD__",
         "__DEFLATE_MATCH_CODE_RUN__",
         symbol(map_text, "__DEFLATE_MATCH_RODATA_LOAD__") +
         symbol(map_text, "__DEFLATE_MATCH_RODATA_SIZE__") -
         symbol(map_text, "__DEFLATE_MATCH_CODE_LOAD__")),
        ("DEFLATE_EMIT_CODE", 0x3200, "__DEFLATE_EMIT_CODE_LOAD__",
         "__DEFLATE_EMIT_CODE_RUN__",
         symbol(map_text, "__DEFLATE_EMIT_RODATA_LOAD__") +
         symbol(map_text, "__DEFLATE_EMIT_RODATA_SIZE__") -
         symbol(map_text, "__DEFLATE_EMIT_CODE_LOAD__")),
        ("DEFLATE_COORD_CODE", 0x3F00, "__DEFLATE_COORD_CODE_LOAD__",
         "__DEFLATE_COORD_CODE_RUN__",
         symbol(map_text, "__DEFLATE_COORD_RODATA_LOAD__") +
         symbol(map_text, "__DEFLATE_COORD_RODATA_SIZE__") -
         symbol(map_text, "__DEFLATE_COORD_CODE_LOAD__")),
        ("ZIP_READ_CODE", 0x4A00, "__ZIP_READ_CODE_LOAD__",
         "__ZIP_READ_CODE_RUN__", symbol(map_text, "__ZIP_READ_CODE_SIZE__")),
    )
    phase_values: dict[str, tuple[int, int, int, int]] = {}
    compact_offset = PACKAGE_HEADER_SIZE
    for name, raw_offset, load_name, run_name, size in phases:
        load_address = symbol(map_text, load_name)
        run_address = symbol(map_text, run_name)
        if not 0 < size <= 0x1500 or raw_offset + size > len(image):
            raise SystemExit(
                f"{name} size/slot invalid: {size} at ${raw_offset:04X}"
            )
        if load_address != raw_offset:
            raise SystemExit(
                f"{name} linked at ${load_address:04X}, "
                f"expected ${raw_offset:04X}"
            )
        phase_values[name] = (compact_offset, size, run_address, raw_offset)
        compact_offset += size

    if compact_offset > PACKAGE_MAX_SIZE:
        raise SystemExit(
            f"compact package is {compact_offset} bytes, max {PACKAGE_MAX_SIZE}"
        )

    package = bytearray(compact_offset)
    for offset, size, _run_address, raw_offset in phase_values.values():
        package[offset:offset + size] = image[raw_offset:raw_offset + size]

    job = phase_values["JOB_CODE"]
    inflate = phase_values["INFLATE_CODE"]
    match = phase_values["DEFLATE_MATCH_CODE"]
    emit = phase_values["DEFLATE_EMIT_CODE"]
    coord = phase_values["DEFLATE_COORD_CODE"]
    reader = phase_values["ZIP_READ_CODE"]
    header = bytearray(PACKAGE_HEADER_SIZE)
    header[0:4] = b"UZPK"
    header[4:6] = bytes((PACKAGE_VERSION, PHASE_COUNT))

    def put16(at: int, value: int) -> None:
        header[at:at + 2] = value.to_bytes(2, "little")

    put16(6, PACKAGE_HEADER_SIZE)
    put16(8, len(package))

    bss_sizes = (
        0,
        symbol(map_text, "__INFLATE_BSS_SIZE__"),
        0,
        0,
        symbol(map_text, "__DEFLATE_COORD_BSS_SIZE__"),
        0,
    )
    ordered = (job, inflate, match, emit, coord, reader)
    for index, (phase, bss_size) in enumerate(zip(ordered, bss_sizes)):
        at = DESCRIPTOR_BASE + index * DESCRIPTOR_SIZE
        put16(at, phase[0])
        put16(at + 2, phase[1])
        put16(at + 4, phase[2])
        put16(at + 6, bss_size)
    package[0:len(header)] = header

    create_size = (
        symbol(map_text, "__CREATE_COORD_RODATA_LOAD__")
        + symbol(map_text, "__CREATE_COORD_RODATA_SIZE__")
        - symbol(map_text, "__CREATE_COORD_CODE_LOAD__")
    )
    create_load = symbol(map_text, "__CREATE_COORD_CODE_LOAD__")
    create_run = symbol(map_text, "__CREATE_COORD_CODE_RUN__")
    create_entry = symbol(map_text, "_uz_create_job_entry")
    if create_load != CREATE_RAW_OFFSET:
        raise SystemExit(
            f"CREATE_COORD_CODE linked at ${create_load:04X}, "
            f"expected ${CREATE_RAW_OFFSET:04X}"
        )
    if not 0 < create_size <= 0x1100:
        raise SystemExit(f"CREATE_COORD_CODE size invalid: {create_size}")
    if not create_run <= create_entry < create_run + create_size:
        raise SystemExit(
            f"create entry ${create_entry:04X} outside "
            f"${create_run:04X}+{create_size}"
        )
    plan_summary = None
    if args.plan_raw is None:
        raise SystemExit("production package requires --plan-raw")
    if args.plan_raw is not None:
        plan_image = args.plan_raw.read_bytes()
        plan_load = symbol(map_text, "__CREATE_PLAN_CODE_LOAD__")
        plan_size = symbol(map_text, "__CREATE_PLAN_CODE_SIZE__")
        plan_rodata_load = optional_symbol(
            map_text, "__CREATE_PLAN_RODATA_LOAD__"
        )
        plan_rodata_size = optional_symbol(
            map_text, "__CREATE_PLAN_RODATA_SIZE__"
        )
        if plan_rodata_load is not None or plan_rodata_size is not None:
            if (plan_rodata_load is None or plan_rodata_size is None or
                    plan_rodata_load != plan_load + plan_size):
                raise SystemExit("CREATE_PLAN_RODATA is not contiguous")
            plan_size += plan_rodata_size
        plan_run = symbol(map_text, "__CREATE_PLAN_CODE_RUN__")
        plan_entry = symbol(map_text, "_uz_create_plan_overlay_entry")
        if plan_load != 0:
            raise SystemExit(
                f"CREATE_PLAN_CODE raw load is ${plan_load:04X}, expected $0000"
            )
        if (not 0 < plan_size <= PLAN_MAX_SIZE or
                len(plan_image) < plan_size or plan_run != PLAN_RUN):
            raise SystemExit(
                f"CREATE_PLAN_CODE image invalid: {plan_size} bytes, "
                f"run ${plan_run:04X}, raw {len(plan_image)} bytes"
            )
        if not plan_run <= plan_entry < plan_run + plan_size:
            raise SystemExit(
                f"plan entry ${plan_entry:04X} outside "
                f"${plan_run:04X}+{plan_size}"
            )
        plan_payload = plan_image[:plan_size]
        create_payload = image[
            CREATE_RAW_OFFSET:CREATE_RAW_OFFSET + create_size
        ]
        bundled = create_payload + plan_payload
        compressor = zlib.compressobj(
            level=9, method=zlib.DEFLATED, wbits=-15, memLevel=9
        )
        bundled_compressed = compressor.compress(bundled) + compressor.flush()
        if zlib.decompress(bundled_compressed, wbits=-15) != bundled:
            raise SystemExit("internal Create/planner Deflate round-trip failed")
        create_header = bytearray(CREATE_HEADER_SIZE)
        create_header[0:4] = CREATE_MAGIC
        create_header[4] = CREATE_VERSION
        create_header[5] = CREATE_HEADER_SIZE
        create_header[6:8] = len(bundled_compressed).to_bytes(2, "little")
        create_header[8:10] = len(bundled).to_bytes(2, "little")
        create_header[10:12] = create_size.to_bytes(2, "little")
        create_header[12:14] = create_run.to_bytes(2, "little")
        create_header[14:16] = create_entry.to_bytes(2, "little")
        create_header[16:18] = create_size.to_bytes(2, "little")
        create_header[18:20] = plan_size.to_bytes(2, "little")
        create_header[20:22] = plan_run.to_bytes(2, "little")
        create_header[22:24] = plan_entry.to_bytes(2, "little")
        create_header[24:28] = (
            binascii.crc32(bundled) & 0xFFFFFFFF
        ).to_bytes(4, "little")
        package.extend(create_header)
        package.extend(bundled_compressed)
        plan_summary = (
            plan_size, plan_run, plan_entry, len(bundled_compressed)
        )

    ui_summary = None
    if args.ui_raw is not None:
        ui_image = args.ui_raw.read_bytes()
        ui_run = 0x3000
        ui_size = len(ui_image)
        ui_entry = symbol(map_text, "_uzip_ui_main")
        if not 0 < ui_size <= 0x7000:
            raise SystemExit(f"UI raw size is invalid: {ui_size}")
        if not ui_run <= ui_entry < ui_run + ui_size:
            raise SystemExit(
                f"UI entry ${ui_entry:04X} outside ${ui_run:04X}+{ui_size}"
            )
        compressor = zlib.compressobj(
            level=9, method=zlib.DEFLATED, wbits=-15, memLevel=9
        )
        ui_compressed = compressor.compress(ui_image) + compressor.flush()
        if zlib.decompress(ui_compressed, wbits=-15) != ui_image:
            raise SystemExit("internal UI Deflate round-trip failed")
        ui_header = bytearray(UI_HEADER_SIZE)
        ui_header[0:4] = UI_MAGIC
        ui_header[4] = UI_VERSION
        ui_header[5] = UI_HEADER_SIZE
        ui_header[6:8] = len(ui_compressed).to_bytes(2, "little")
        ui_header[8:10] = ui_size.to_bytes(2, "little")
        ui_header[10:12] = ui_run.to_bytes(2, "little")
        ui_header[12:14] = ui_entry.to_bytes(2, "little")
        ui_header[14:18] = (binascii.crc32(ui_image) & 0xFFFFFFFF).to_bytes(
            4, "little"
        )
        package.extend(ui_header)
        package.extend(ui_compressed)
        if len(package) > RESOURCE_MAX_SIZE:
            raise SystemExit(
                f"package with cold UI is {len(package)} bytes, "
                f"max {RESOURCE_MAX_SIZE}"
            )
        ui_summary = (ui_size, len(ui_compressed), ui_run, ui_entry)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(b"\x00\x00" + package)
    print(f"uZPK v{PACKAGE_VERSION}: {args.output} "
          f"({len(package)} total payload bytes; "
          f"{compact_offset} frozen-v7 bytes)")
    for name, (offset, size, run, _raw_offset) in phase_values.items():
        print(f"  {name}: bank ${offset:04X}, {size} bytes, run ${run:04X}")
    print(f"  CREATE_COORD_CODE: extension ${compact_offset:04X}, "
          f"{create_size} bytes, run ${create_run:04X}, "
          f"entry ${create_entry:04X}")
    if plan_summary is not None:
        plan_size, plan_run, plan_entry, compressed_size = plan_summary
        print(f"  CREATE/PLAN bundle: {create_size + plan_size} bytes -> "
              f"{compressed_size} Deflate bytes")
        print(f"  CREATE_PLAN_CODE: {plan_size} bytes, run ${plan_run:04X}, "
              f"entry ${plan_entry:04X}")
    if ui_summary is not None:
        ui_size, compressed_size, ui_run, ui_entry = ui_summary
        print(f"  UI_CODE: {ui_size} bytes -> {compressed_size} Deflate bytes, "
              f"run ${ui_run:04X}, entry ${ui_entry:04X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
