#!/usr/bin/env python3
"""Verify the Ultimate ZIP memory and SKU packaging contracts.

This is intentionally a static/build-artifact gate. ZIP behavior is validated
only by the dedicated probes and final ReadyOS app on physical C64 Ultimate
hardware; this script never starts an emulator.
"""

from __future__ import annotations

import json
import hashlib
import re
import sys
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "obj" / "uzip.map"
PRG_PATH = ROOT / "bin" / "uzip.prg"
PACKAGE_PATH = ROOT / "bin" / "uzpack.prg"
PACKAGE_RAW_PATH = ROOT / "bin" / "uzpack.raw"
CFG_PATH = ROOT / "cfg" / "ready_app_uzip_cold.cfg"
DIAGNOSTIC_CFG_PATH = ROOT / "cfg" / "ready_app_uzip.cfg"
UI_RAW_PATH = ROOT / "bin" / "uzui.raw"
PLAN_RAW_PATH = ROOT / "bin" / "uzplan.raw"
ULTIMATE_INI = ROOT / "cfg" / "profiles" / "precog-ultimate.ini"
ULTIMATE_JSON = ROOT / "cfg" / "profiles" / "precog-ultimate.json"

APP_START = 0x1000
CORE_END = 0x2FFF
DICTIONARY_START = 0x3000
DICTIONARY_END = 0xAFFF
UI_START = 0x3000
UI_CODE_END = 0x9FFF
UI_BSS_START = 0xB000
JOB_START = 0xB000
JOB_END = 0xC3FF
STACK_START = 0xC400
STACK_END = 0xC5FF
APP_END = 0xC5FF
SHIM_START = 0xC600
SHIM_END = 0xC9FF
FROZEN_V7_SIZE = 21677
FROZEN_V7_SHA256 = "d9614ec173ed7133bcb7e58b631370042520377b4bb569d0bdd834ff67a4cf11"


def fail(message: str) -> None:
    print(f"[FAIL] {message}")


def passed(message: str) -> None:
    print(f"[OK] {message}")


def parse_segments(path: Path) -> dict[str, tuple[int, int, int]]:
    source = path.read_text(encoding="utf-8", errors="replace")
    pattern = re.compile(
        r"^\s*([A-Z0-9_]+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+"
        r"([0-9A-F]{6})\s+[0-9A-F]{5}\s*$"
    )
    segments: dict[str, tuple[int, int, int]] = {}
    in_list = False
    for line in source.splitlines():
        if line.strip() == "Segment list:":
            in_list = True
            continue
        if not in_list:
            continue
        match = pattern.match(line)
        if match:
            segments[match.group(1)] = tuple(
                int(match.group(index), 16) for index in range(2, 5)
            )
        elif segments and not line.strip():
            break
    if not segments:
        raise ValueError(f"segment list missing from {path}")
    return segments


def parse_symbol(path: Path, name: str) -> int:
    source = path.read_text(encoding="utf-8", errors="replace")
    match = re.search(
        rf"(?:^|\s){re.escape(name)}\s+([0-9A-F]+)\s", source, re.MULTILINE
    )
    if not match:
        raise ValueError(f"symbol {name} missing from {path}")
    return int(match.group(1), 16)


def check_segment_range(
    segments: dict[str, tuple[int, int, int]],
    names: tuple[str, ...],
    low: int,
    high: int,
) -> bool:
    ok = True
    for name in names:
        if name not in segments:
            continue
        start, end, _ = segments[name]
        if start < low or end > high:
            fail(f"{name} ${start:04X}-${end:04X} escapes ${low:04X}-${high:04X}")
            ok = False
    return ok


def main() -> int:
    ok = True
    for path in (MAP_PATH, PRG_PATH, PACKAGE_PATH, PACKAGE_RAW_PATH, CFG_PATH,
                 DIAGNOSTIC_CFG_PATH, ULTIMATE_INI, ULTIMATE_JSON):
        if not path.exists():
            fail(f"missing required file: {path.relative_to(ROOT)}")
            ok = False
    if not ok:
        return 1

    try:
        segments = parse_segments(MAP_PATH)
        main_start = parse_symbol(MAP_PATH, "__MAIN_START__")
        main_size = parse_symbol(MAP_PATH, "__MAIN_SIZE__")
    except ValueError as error:
        fail(str(error))
        return 1
    map_source = MAP_PATH.read_text(encoding="utf-8", errors="replace")
    physical_diagnostic = any(name in map_source for name in (
        "_xuzreu_diag_run", "_xuzdeflate_diag_run", "_xuzzip8_diag_run",
        "_xuzmulti_diag_run", "_xuzread_diag_run",
        "_xuzextract_diag_run", "_xuzcreateplan_diag_run",
    ))
    if not physical_diagnostic and not UI_RAW_PATH.exists():
        fail(f"missing required file: {UI_RAW_PATH.relative_to(ROOT)}")
        return 1
    if not physical_diagnostic and not PLAN_RAW_PATH.exists():
        fail(f"missing required file: {PLAN_RAW_PATH.relative_to(ROOT)}")
        return 1

    if main_start + main_size == SHIM_START:
        passed("cc65 crt0 stack top is the ReadyOS shim boundary $C600")
    else:
        fail(
            f"cc65 crt0 stack top is ${main_start + main_size:04X}, "
            "expected $C600"
        )
        ok = False

    required = ("STARTUP", "CODE", "RODATA", "DATA", "BSS")
    missing = [name for name in required if name not in segments]
    if missing:
        fail("missing linker segments: " + ", ".join(missing))
        ok = False
    else:
        passed("required resident linker segments are present")

    resident_names = ("STARTUP", "LOWCODE", "CODE", "RODATA", "DATA", "INIT", "ONCE", "BSS",
                      "COLD_CODE", "COLD_DATA")
    if check_segment_range(segments, resident_names, APP_START, CORE_END):
        resident_end = max(segments[name][1] for name in resident_names if name in segments)
        passed(
            f"resident core ends at ${resident_end:04X}; "
            f"{CORE_END - resident_end:,} bytes remain below dictionary window"
        )
    else:
        ok = False

    if check_segment_range(segments, ("UI_CODE", "UI_RODATA"), UI_START, UI_CODE_END):
        passed("idle UI code and rodata stay in $3000-$9FFF")
    else:
        ok = False
    if not physical_diagnostic:
        if check_segment_range(segments, ("BOOT_CODE", "BOOT_BSS"), 0xA000, 0xAFFF):
            passed("cold UI bootstrap stays in its temporary $A000-$AFFF window")
        else:
            ok = False
        if check_segment_range(
            segments, ("CREATE_PLAN_CODE", "CREATE_PLAN_RODATA"),
            0x9000, 0xAFFF,
        ):
            plan_image_size = sum(
                segments[name][2]
                for name in ("CREATE_PLAN_CODE", "CREATE_PLAN_RODATA")
                if name in segments
            )
            if 0 < plan_image_size <= 0x2000:
                passed(
                    f"recursive planner is {plan_image_size:,} bytes in its "
                    "$9000-$AFFF modal window"
                )
            else:
                fail(f"recursive planner is {plan_image_size:,} bytes")
                ok = False
        else:
            ok = False
        if check_segment_range(segments, ("CREATE_PLAN_BSS",), 0x0800, 0x0BFF):
            passed("recursive planner BSS stays in its modal low-memory window")
        else:
            ok = False
    if check_segment_range(
        segments,
        (
            "JOB_CODE", "JOB_RODATA", "CODESEED",
            "DEFLATE_MATCH_CODE", "DEFLATE_MATCH_RODATA",
            "DEFLATE_EMIT_CODE", "DEFLATE_EMIT_RODATA",
            "ZIP_READ_CODE",
            "INFLATE_CODE", "INFLATE_RODATA", "INFLATE_BSS",
        ),
        JOB_START,
        JOB_END,
    ):
        passed("all mutually exclusive job images stay in $B000-$C3FF")
    else:
        ok = False
    if check_segment_range(
        segments, ("DEFLATE_COORD_CODE", "DEFLATE_COORD_RODATA",
                   "DEFLATE_COORD_BSS"),
        0xA000, 0xAFFF,
    ):
        coord_size = sum(
            segments[name][2]
            for name in ("DEFLATE_COORD_CODE", "DEFLATE_COORD_RODATA",
                         "DEFLATE_COORD_BSS")
            if name in segments
        )
        if coord_size == 0 or coord_size > 0x1000:
            fail(f"Deflate coordinator is {coord_size:,} bytes; expected 1-4096")
            ok = False
        else:
            passed(f"Deflate coordinator is {coord_size:,} bytes at $A000-$AFFF")
    else:
        ok = False
    if "CREATE_COORD_CODE" in segments:
        if check_segment_range(
            segments, ("CREATE_COORD_CODE", "CREATE_COORD_RODATA"),
            0xA000, 0xB0FF,
        ):
            create_size = sum(
                segments[name][2]
                for name in ("CREATE_COORD_CODE", "CREATE_COORD_RODATA")
                if name in segments
            )
            if 0 < create_size <= 0x1100:
                passed(
                    f"create coordinator is {create_size:,} bytes in its "
                    "$A000 alternate window"
                )
            else:
                fail(f"create coordinator is {create_size:,} bytes")
                ok = False
        else:
            ok = False
    archive_creation_diagnostic = (
        "_xuzzip8_diag_run" in map_source or
        "_xuzmulti_diag_run" in map_source
    )
    zip_reader_diagnostic = "_xuzread_diag_run" in map_source
    reu_diagnostic = "_xuzreu_diag_run" in map_source
    deflate_diagnostic = "_xuzdeflate_diag_run" in map_source
    create_plan_diagnostic = "_xuzcreateplan_diag_run" in map_source
    self_seed_diagnostic = (
        archive_creation_diagnostic or zip_reader_diagnostic or
        reu_diagnostic or deflate_diagnostic or create_plan_diagnostic
    )
    if "_xuzdeflate_coord_entry" in map_source:
        try:
            diagnostic_entry = parse_symbol(MAP_PATH, "_xuzdeflate_coord_entry")
        except ValueError as error:
            fail(str(error))
            ok = False
        else:
            coord_start, coord_end, _ = segments["DEFLATE_COORD_CODE"]
            if coord_start <= diagnostic_entry <= coord_end:
                passed(
                    f"diagnostic coordinator entry ${diagnostic_entry:04X} "
                    f"lies inside loaded image ${coord_start:04X}-${coord_end:04X}"
                )
            else:
                fail(
                    f"diagnostic coordinator entry ${diagnostic_entry:04X} "
                    "lies outside its loaded image"
                )
                ok = False
    for label, names in (
        ("Deflate matcher", ("DEFLATE_MATCH_CODE", "DEFLATE_MATCH_RODATA")),
        ("Deflate emitter", ("DEFLATE_EMIT_CODE", "DEFLATE_EMIT_RODATA")),
        ("ZIP reader", ("ZIP_READ_CODE",)),
        ("Inflater", ("INFLATE_CODE", "INFLATE_RODATA")),
    ):
        if any(name not in segments for name in names):
            fail(f"{label} packed phase is missing")
            ok = False
            continue
        phase_size = sum(segments[name][2] for name in names)
        limit = {
            "Deflate matcher": 0x1000,
            "Deflate emitter": 0x0D00,
            "ZIP reader": 0x1500,
            "Inflater": 0x1000,
        }[label]
        size_valid = phase_size <= limit
        if label == "Inflater":
            size_valid = (phase_size == 2 if self_seed_diagnostic
                          else 0x0800 <= phase_size <= limit)
        elif (label in ("Deflate matcher", "Deflate emitter") and
              (zip_reader_diagnostic or reu_diagnostic)):
            size_valid = phase_size == 2
        elif label == "ZIP reader":
            size_valid = (phase_size <= 0x0100 if (
                              archive_creation_diagnostic or reu_diagnostic or
                              deflate_diagnostic or create_plan_diagnostic)
                          else 0x0800 <= phase_size <= limit)
        if segments[names[0]][0] == JOB_START and size_valid:
            passed(
                f"{label} phase is {phase_size:,} bytes within its "
                f"{limit // 1024}K package slot"
            )
        else:
            fail(f"{label} phase is {phase_size:,} bytes or has the wrong run base")
            ok = False
    if check_segment_range(segments, ("UI_BSS",), UI_BSS_START, JOB_END):
        passed("idle UI BSS stays in $A000-$C3FF")
    else:
        ok = False
    _, ui_bss_end, _ = segments["UI_BSS"]
    if ui_bss_end <= JOB_END:
        passed("UI BSS is covered by the $3000-$C3FF job snapshot")
    else:
        fail("UI BSS extends beyond the restored job snapshot")
        ok = False

    for name, (start, end, _) in segments.items():
        if name in ("ZEROPAGE", "LOADADDR"):
            continue
        if not (end < SHIM_START or start > SHIM_END):
            fail(f"{name} overlaps resident shim ${SHIM_START:04X}-${SHIM_END:04X}")
            ok = False
    if ok:
        passed("no uZIP segment overlaps the ReadyOS shim")

    prg = PRG_PATH.read_bytes()
    load = prg[0] | (prg[1] << 8)
    end = load + len(prg) - 3
    map_source = MAP_PATH.read_text(encoding="utf-8", errors="replace")
    ui_size = sum(segments[name][2] for name in ("UI_CODE", "UI_RODATA")
                  if name in segments)
    cold_boot_size = segments.get("BOOT_CODE", (0, 0, 0))[2]
    expected_end = DICTIONARY_START + cold_boot_size - 1
    if physical_diagnostic and load == APP_START and end < 0xB000:
        passed(
            f"physical diagnostic PRG stays in its cold load window: "
            f"${load:04X}-${end:04X}"
        )
    elif (load == APP_START and cold_boot_size != 0 and end == expected_end and
          end < 0x4000):
        passed(
            f"PRG contains only resident core plus {cold_boot_size:,}-byte "
            f"UI bootstrap: ${load:04X}-${end:04X}"
        )
    else:
        fail(
            f"PRG load span is ${load:04X}-${end:04X}, expected compact end "
            f"${expected_end:04X} (and it must remain below $A000)"
        )
        ok = False

    package = PACKAGE_PATH.read_bytes()
    payload = package[2:] if len(package) >= 2 else b""
    v7_size = int.from_bytes(payload[8:10], "little") if len(payload) >= 10 else 0
    package_ok = (
        len(payload) >= 64 and len(payload) <= 0xD000 and
        package[:2] == b"\x00\x00" and payload[:6] == b"UZPK\x07\x06" and
        int.from_bytes(payload[6:8], "little") == 64 and
        64 <= v7_size <= 0x5F00 and v7_size <= len(payload) and
        payload[10:12] == b"\x00\x00" and payload[60:64] == b"\x00" * 4
    )
    cursor = 64
    if package_ok:
        for phase in range(6):
            at = 12 + phase * 8
            offset = int.from_bytes(payload[at:at + 2], "little")
            size = int.from_bytes(payload[at + 2:at + 4], "little")
            run = int.from_bytes(payload[at + 4:at + 6], "little")
            if (offset != cursor or size == 0 or cursor + size > v7_size or
                    run != (0xA000 if phase == 4 else 0xB000)):
                package_ok = False
                break
            cursor += size
        package_ok = package_ok and cursor == v7_size
    bundle_end = v7_size
    if package_ok and len(payload) != v7_size:
        extension = payload[v7_size:]
        bundle_compressed_size = (int.from_bytes(extension[6:8], "little")
                                  if len(extension) >= 28 else 0)
        bundle_output_size = (int.from_bytes(extension[8:10], "little")
                              if len(extension) >= 28 else 0)
        create_size = (int.from_bytes(extension[10:12], "little")
                       if len(extension) >= 28 else 0)
        create_run = (int.from_bytes(extension[12:14], "little")
                      if len(extension) >= 28 else 0)
        create_entry = (int.from_bytes(extension[14:16], "little")
                        if len(extension) >= 28 else 0)
        plan_offset = (int.from_bytes(extension[16:18], "little")
                       if len(extension) >= 28 else 0)
        plan_size_header = (int.from_bytes(extension[18:20], "little")
                            if len(extension) >= 28 else 0)
        plan_run = (int.from_bytes(extension[20:22], "little")
                    if len(extension) >= 28 else 0)
        plan_entry = (int.from_bytes(extension[22:24], "little")
                      if len(extension) >= 28 else 0)
        bundle_crc = (int.from_bytes(extension[24:28], "little")
                      if len(extension) >= 28 else 0)
        compressed_bundle = extension[28:28 + bundle_compressed_size]
        try:
            expanded_bundle = zlib.decompress(compressed_bundle, wbits=-15)
        except zlib.error:
            expanded_bundle = b""
        plan_raw = PLAN_RAW_PATH.read_bytes()
        package_raw = PACKAGE_RAW_PATH.read_bytes()
        package_ok = (
            len(extension) >= 28 + bundle_compressed_size and
            extension[:6] == b"UZCR\x02\x1c" and
            bundle_compressed_size > 0 and
            len(expanded_bundle) == bundle_output_size and
            0 < create_size <= 0x1100 and create_run == 0xA000 and
            create_run <= create_entry < create_run + create_size and
            plan_offset == create_size and
            0 < plan_size_header <= 0x2000 and
            bundle_output_size == create_size + plan_size_header and
            plan_run == 0x9000 and
            plan_run <= plan_entry < plan_run + plan_size_header and
            expanded_bundle[:create_size] ==
                package_raw[0x5F00:0x5F00 + create_size] and
            expanded_bundle[plan_offset:] == plan_raw[:plan_size_header] and
            (zlib.crc32(expanded_bundle) & 0xFFFFFFFF) == bundle_crc
        )
        bundle_end = v7_size + 28 + bundle_compressed_size
    ui_raw = UI_RAW_PATH.read_bytes() if not physical_diagnostic else b""
    ui_compressed_size = 0
    if package_ok and not physical_diagnostic:
        ui_extension = payload[bundle_end:]
        ui_compressed_size = (int.from_bytes(ui_extension[6:8], "little")
                              if len(ui_extension) >= 20 else 0)
        ui_size_header = (int.from_bytes(ui_extension[8:10], "little")
                          if len(ui_extension) >= 20 else 0)
        ui_run = (int.from_bytes(ui_extension[10:12], "little")
                  if len(ui_extension) >= 20 else 0)
        ui_entry = (int.from_bytes(ui_extension[12:14], "little")
                    if len(ui_extension) >= 20 else 0)
        ui_crc = (int.from_bytes(ui_extension[14:18], "little")
                  if len(ui_extension) >= 20 else 0)
        compressed = ui_extension[20:]
        try:
            expanded = zlib.decompress(compressed, wbits=-15)
        except zlib.error:
            expanded = b""
        package_ok = (
            ui_extension[:6] == b"UZUI\x01\x14" and
            len(compressed) == ui_compressed_size and
            ui_size_header == len(ui_raw) == ui_size and
            ui_run == UI_START and ui_run <= ui_entry < ui_run + ui_size and
            expanded == ui_raw and
            (zlib.crc32(ui_raw) & 0xFFFFFFFF) == ui_crc
        )
    if package_ok and not physical_diagnostic:
        frozen_digest = hashlib.sha256(payload[:v7_size]).hexdigest()
        if v7_size != FROZEN_V7_SIZE or frozen_digest != FROZEN_V7_SHA256:
            fail("canonical v7 extraction prefix no longer matches the frozen baseline")
            package_ok = False
        if len(payload) > 0xD000:
            fail("on-disk package overlaps the $D000 planner cache")
            package_ok = False
    package_is_selfseed_stub = self_seed_diagnostic and package == b"\x00\x00\x00"
    if package_is_selfseed_stub:
        passed("self-seeded diagnostic uses a one-byte launcher resource placeholder")
    elif package_ok:
        passed(
            f"external uzpack.prg keeps the exact frozen uZPK v7 prefix "
            f"({v7_size:,} bytes), compressed Create/planner bundle, and "
            f"{ui_compressed_size:,}-byte cold UI stream"
        )
    else:
        fail("external uzpack.prg compact v7 header/layout is invalid")
        ok = False

    cfg_path = DIAGNOSTIC_CFG_PATH if physical_diagnostic else CFG_PATH
    cfg = cfg_path.read_text(encoding="utf-8", errors="replace")
    cfg_needles = [
        "value = $0200",
        "value = $C600",
        "MAIN:      file = \"\", start = $1000, size = $B600",
        "start = __ONCE_RUN__ + __ONCE_SIZE__",
        "start = $B000, size = $1400",
        "start = $B000, size = $1400",
        "start = $A000, size = $1000",
        'PK_HEADER: file = "bin/uzpack.raw"',
        "PK_JOB:    file = \"bin/uzpack.raw\"",
        "load = PK_JOB, run = JOB",
        "load = PK_INFLATE, run = JOB_INFLATE",
        "load = PK_READ, run = JOB_READ",
    ]
    if not physical_diagnostic:
        cfg_needles.extend((
            "start = $1000, size = $1F00",
            "COLDCORE:  file = %O, start = $2F00, size = $0100",
            'UIRAW:     file = "bin/uzui.raw"',
            'PLANRAW:   file = "bin/uzplan.raw"',
            'PLANRUN:   file = "", start = $9000, size = $2000',
            'PLANBSS:   file = "", start = $0800, size = $0400',
            "BOOTRUN:   file = \"\", start = $A000, size = $1000",
            "BOOT_CODE: load = BOOTLOAD, run = BOOTRUN",
            "UI_CODE:   load = UIRAW, run = UI",
            "CREATE_PLAN_CODE: load = PLANRAW, run = PLANRUN",
            "CREATE_PLAN_BSS: load = PLANBSS",
        ))
    absent = [needle for needle in cfg_needles if needle not in cfg]
    if absent:
        fail("linker contract markers missing: " + ", ".join(absent))
        ok = False
    else:
        passed("linker config retains the uZIP mode-window contract")

    pack_asm = (ROOT / "src" / "apps" / "uzip" / "uz_pack_asm.s").read_text(
        encoding="utf-8", errors="replace"
    )
    pack_needles = (
        "__JOB_CODE_LOAD__",
        "__JOB_CODE_RUN__",
        "__JOB_RODATA_LOAD__ + __JOB_RODATA_SIZE__ - __JOB_CODE_LOAD__",
        "__DEFLATE_MATCH_CODE_LOAD__",
        "__DEFLATE_MATCH_RODATA_SIZE__",
        "__DEFLATE_EMIT_CODE_LOAD__",
        "__DEFLATE_EMIT_RODATA_SIZE__",
        "__DEFLATE_COORD_CODE_LOAD__",
        "__DEFLATE_COORD_RODATA_SIZE__",
        "__DEFLATE_COORD_BSS_RUN__",
        "__DEFLATE_COORD_BSS_SIZE__",
        "__ZIP_READ_CODE_LOAD__",
        "__ZIP_READ_CODE_SIZE__",
    )
    if all(needle in pack_asm for needle in pack_needles):
        passed("packed job seed uses linker-generated load/run/size metadata")
    else:
        fail("packed job seed bridge no longer covers the complete phase image")
        ok = False

    deflate_header = (ROOT / "src" / "apps" / "uzip" / "uz_deflate.h").read_text(
        encoding="utf-8", errors="replace"
    )
    deflate_source = (ROOT / "src" / "apps" / "uzip" / "uz_deflate.c").read_text(
        encoding="utf-8", errors="replace"
    )
    inflate_source = (ROOT / "src" / "apps" / "uzip" / "uz_inflate6502.c").read_text(
        encoding="utf-8", errors="replace"
    )
    if ("UZ_DEFLATE_BLOCK_SIZE    2048u" in deflate_header and
            "UZ_DEFLATE_TOKEN_MAX     4096u" in deflate_header):
        passed("compressor retains the 2K input / 4K worst-token contract")
    else:
        fail("compressor block/token contract changed without a new memory proof")
        ok = False
    if ("uz_deflate_match_crc_update" in deflate_source and
            "inflate_crc_update" in inflate_source and
            "uz_crc32_update(" not in deflate_source and
            "uz_crc32_update(" not in inflate_source):
        passed("active compressor/inflater images own their CRC call edges")
    else:
        fail("a codec phase again depends on the Store overlay's CRC symbol")
        ok = False

    app_source = (ROOT / "src" / "apps" / "uzip" / "uzip.c").read_text(
        encoding="utf-8", errors="replace"
    )
    cold_boot_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_cold_boot.c"
    ).read_text(encoding="utf-8", errors="replace")
    plan_overlay_header = (
        ROOT / "src" / "apps" / "uzip" / "uz_create_plan_overlay.h"
    ).read_text(encoding="utf-8", errors="replace")
    planner_overlay_needles = (
        "UZ_CREATE_PLAN_OVERLAY_CACHE_OFFSET 0xD000u",
        "UZ_CREATE_PLAN_OVERLAY_SAVE_OFFSET  0xD000u",
        "reu_dma_stash((unsigned int)(0x3000u + plan_offset)",
        "UZ_CREATE_PLAN_OVERLAY_CACHE_OFFSET, plan_size",
        "reu_dma_stash(UZ_CREATE_PLAN_OVERLAY_RUN, work_bank",
        "reu_dma_fetch(UZ_CREATE_PLAN_OVERLAY_RUN, package_bank",
        "result = uz_create_plan_overlay_entry(&plan_request);",
        "reu_dma_fetch(UZ_CREATE_PLAN_OVERLAY_RUN, work_bank",
    )
    planner_overlay_sources = (
        plan_overlay_header + "\n" + cold_boot_source + "\n" + app_source
    )
    if all(needle in planner_overlay_sources for needle in planner_overlay_needles):
        passed("modal planner caches in package REU and restores its overlapping UI tail")
    else:
        fail("modal planner can overwrite the idle UI or collide with package bytes")
        ok = False
    package_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_package.c"
    ).read_text(encoding="utf-8", errors="replace")
    if ('static unsigned char package_header[UZ_PACKAGE_HEADER_SIZE];' in
            package_source and
            '#pragma bss-name(push, "UI_BSS")' not in package_source and
            "cached header must remain in resident BSS below $3000" in
            package_source):
        passed("compact package descriptors survive every destructive overlay")
    else:
        fail("compact package descriptors can be overwritten by an overlay")
        ok = False
    package_header_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_package.h"
    ).read_text(encoding="utf-8", errors="replace")
    job_source = (ROOT / "src" / "apps" / "uzip" / "uz_job.c").read_text(
        encoding="utf-8", errors="replace"
    )
    package_needles = (
        "#define UZ_PACKAGE_VERSION       7u",
        "#define UZ_PACKAGE_HEADER_SIZE   64u",
        "offset != cursor",
        "cursor != payload_size",
        "seed_descriptor(UZ_PACKAGE_PHASE_COORD",
        "seed_descriptor(UZ_PACKAGE_PHASE_READER",
        "uz_package_phase_offset(UZ_PACKAGE_PHASE_COORD)",
        "uz_package_phase_offset(UZ_PACKAGE_PHASE_INFLATE)",
        "coord_size > 0x0B00u",
        "zip_read_size > 0x1500u",
        "uz_pack_deflate_coord_run() != 0xA000u",
        "uz_pack_deflate_coord_load()",
        "uz_pack_zip_read_run() != 0xB000u",
        "uz_pack_zip_read_load()",
    )
    package_sources = (package_header_source + "\n" + package_source + "\n" +
                       app_source + "\n" + job_source)
    if all(needle in package_sources for needle in package_needles):
        passed("uZPK v7 validates and loads six consecutive phase images")
    else:
        fail("uZPK v7 descriptor/seeding/loading contract is incomplete")
        ok = False

    diagnostic_ui_source = (
        ROOT / "src" / "apps" / "uzip" / "xuzdeflate_diag_ui.c"
    ).read_text(encoding="utf-8", errors="replace")
    coord_source = (
        ROOT / "src" / "apps" / "uzip" / "xuzdeflate_diag_coord.c"
    ).read_text(encoding="utf-8", errors="replace")
    zip_write_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_zip_write.c"
    ).read_text(encoding="utf-8", errors="replace")
    zip8_analyzer_source = (
        ROOT / "build_support" / "analyze_xuzzip8_outputs.py"
    ).read_text(encoding="utf-8", errors="replace")
    deflate_analyzer_source = (
        ROOT / "build_support" / "analyze_xuzdeflate_run.py"
    ).read_text(encoding="utf-8", errors="replace")
    zip8_runner_source = (
        ROOT / "build_support" / "run_xuzdeflate_c64u.sh"
    ).read_text(encoding="utf-8", errors="replace")
    zip8_needles = (
        "record->method = method",
        "put16(header + 8u, method)",
        "put16(header + 10u, record->method)",
        "uz_u32_add(&writer->offset, &writer->active->compressed_size)",
        "uz_zip_begin_deflate(XUZD_ZIP_WRITER, XUZD_ZIP_RECORD",
        "uz_zip_finish_deflate(XUZD_ZIP_WRITER)",
        "uz_zip_emit_central(XUZD_ZIP_WRITER, XUZD_ZIP_RECORD)",
        "uz_zip_finish_archive(XUZD_ZIP_WRITER, &central_offset, 1u)",
        "XUZD_REU_ZIP_STATE_OFFSET",
        "reu_alloc_owned_bank(XUZD_CATALOG_SLOT, \"uzct\")",
        "catalog_bank == work_bank",
        "catalog_stash(0u, XUZD_ZIP_RECORD)",
        "catalog_fetch(0u, XUZD_ZIP_RECORD)",
        "result[4] >= 2 and result[23] != 1",
        "data[descriptor:descriptor + 4] == b\"PK\\x07\\x08\"",
        "XUZDEFLATE_ARCHIVE_MODE",
    )
    zip8_sources = (zip_write_source + "\n" + diagnostic_ui_source + "\n" +
                    coord_source + "\n" + zip8_analyzer_source + "\n" +
                    deflate_analyzer_source + "\n" + zip8_runner_source)
    if all(needle in zip8_sources for needle in zip8_needles):
        passed("method-8 probe brackets the raw job with Store and a byte oracle")
    else:
        fail("method-8 Store/job/Store physical contract is incomplete")
        ok = False

    multi_analyzer_source = (
        ROOT / "build_support" / "analyze_xuzmulti_outputs.py"
    ).read_text(encoding="utf-8", errors="replace")
    multi_needles = (
        "#define XUZD_ENTRY_COUNT 7u",
        'return "root/sub/random.bin"',
        'return "root/repeat.sto"',
        "XUZD_CONTEXT->entry_index = index",
        "XUZD_CONTEXT->last_entry",
        "strcpy((char *)XUZD_WORKSPACE, (const char *)XUZD_INPUT)",
        "uz_zip_store_data(XUZD_ZIP_WRITER",
        "catalog_stash(XUZD_CONTEXT->entry_index",
        "catalog_index < XUZD_CONTEXT->entry_count",
        "&central_offset,\n                                             XUZD_CONTEXT->entry_count",
        'zipmulti)',
        '"ROOT/SUB/RANDOM.BIN"',
        '"ROOT/REPEAT.STO"',
        'data[descriptor:descriptor + 4] == b"PK\\x07\\x08"',
        'archive.read(info) == plain',
    )
    multi_sources = (diagnostic_ui_source + "\n" + coord_source + "\n" +
                     zip8_runner_source + "\n" + multi_analyzer_source)
    if all(needle in multi_sources for needle in multi_needles):
        passed("mixed seven-entry probe streams one archive with a strict oracle")
    else:
        fail("mixed nested archive physical contract is incomplete")
        ok = False

    make_source = (ROOT / "Makefile").read_text(encoding="utf-8", errors="replace")
    zipread_source = (
        ROOT / "src" / "apps" / "uzip" / "xuzread_diag.c"
    ).read_text(encoding="utf-8", errors="replace")
    zipread_runner = (
        ROOT / "build_support" / "run_xuzread_c64u.sh"
    ).read_text(encoding="utf-8", errors="replace")
    zipread_analyzer = (
        ROOT / "build_support" / "analyze_xuzread_run.py"
    ).read_text(encoding="utf-8", errors="replace")
    zipread_needles = (
        "UZIP_ZIPREAD_DIAGNOSTIC ?= 0",
        "-DUZ_ZIP_READ_CALLBACK_ONLY -DUZ_ZIP_READ_PARSER_ONLY",
        "uz_dos_seek(XUZR_DOS, offset)",
        "uz_dos_load_reu(XUZR_DOS, work_bank",
        "uz_zip_reader_init_at(XUZR_READER, archive_size, read_at, 0)",
        "uz_zip_reader_begin(XUZR_READER",
        "uz_zip_reader_next(XUZR_READER",
        "uz_zip_reader_finished(XUZR_READER)",
        "reu_dma_stash((unsigned int)XUZR_RECORD, catalog_bank",
        "reu_dma_fetch((unsigned int)XUZR_RECORD, catalog_bank",
        "UZIP_ZIPREAD_DIAGNOSTIC=1",
        "build_xuzread_fixture.py",
        "run-ultimate-plan --plan",
        'raw.find(b"XZP1")',
        '"catalog_roundtrip": "pass"',
    )
    zipread_sources = (make_source + "\n" + zipread_source + "\n" +
                       zipread_runner + "\n" + zipread_analyzer)
    if all(needle in zipread_sources for needle in zipread_needles):
        passed("physical ZIP-reader probe preserves random-access and catalog boundaries")
    else:
        fail("ZIP-reader physical probe contract is incomplete")
        ok = False

    extract_source = (
        ROOT / "src" / "apps" / "uzip" / "xuzextract_diag.c"
    ).read_text(encoding="utf-8", errors="replace")
    extract_fs_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_extract_fs.c"
    ).read_text(encoding="utf-8", errors="replace")
    extract_runner = (
        ROOT / "build_support" / "run_xuzextract_c64u.sh"
    ).read_text(encoding="utf-8", errors="replace")
    extract_analyzer = (
        ROOT / "build_support" / "analyze_xuzextract_run.py"
    ).read_text(encoding="utf-8", errors="replace")
    extract_needles = (
        "UZIP_EXTRACT_DIAGNOSTIC ?= 0",
        "preflight(package_bank, work_bank, catalog_bank,",
        "XUZE_GOOD_ARCHIVE, 0u",
        "XUZE_BAD_ARCHIVE, 1u",
        "extract_good(package_bank, work_bank, catalog_bank)",
        "extract_bad_crc(package_bank, work_bank, catalog_bank)",
        "uz_extract_member(&extract_state",
        "static const unsigned char owner_bytes[XUZE_OWNER_LENGTH]",
        "XUZE_P_DATA[index] != owner_bytes[index]",
        "UZ_EXTRACT_COMMIT",
        "UZ_INFLATE_JOB_CRC",
        "state->job_error = job_result ? UZ_INFLATE_JOB_OK :\n"
        "                           uz_job_inflate_error();",
        "XUZEXTRACT_SPEED_MHZ",
        "run-ultimate-plan --plan",
        "VICE use: forbidden",
        '(args.downloads / "STORE.BIN").read_bytes()',
        '(args.downloads / "DEFLATE.BIN").read_bytes()',
        '"existing destination sentinel was changed"',
        'name.startswith(".UZTMP")',
    )
    extract_sources = (make_source + "\n" + extract_source + "\n" +
                       extract_fs_source + "\n" + extract_runner + "\n" +
                       extract_analyzer)
    if all(needle in extract_sources for needle in extract_needles):
        passed("physical extraction probe covers preflight, codecs, commit, and cleanup")
    else:
        fail("physical extraction transaction probe contract is incomplete")
        ok = False

    workflow_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_workflow.c"
    ).read_text(encoding="utf-8", errors="replace")
    create_job_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_create_job.c"
    ).read_text(encoding="utf-8", errors="replace")
    workflow_handoff_needles = (
        "static UzDos extract_input_dos;",
        "static UzDos extract_output_dos;",
        "static void init_extract_dos(void)",
        "inflater's $0400-$07FB input/output buffers",
        "&extract_input_dos, &extract_output_dos",
    )
    if all(needle in workflow_source for needle in workflow_handoff_needles):
        passed("production extraction handles survive the inflater workspace")
    else:
        fail("production extraction handles can overlap the inflater workspace")
        ok = False

    recursive_create_needles = (
        "for (index = 0u; index < entry_count; ++index)",
        "progress(progress_context, index, entry_count",
        "UZ_CREATE_JOB_REQUEST->first_entry",
        "UZ_CREATE_JOB_REQUEST->last_entry",
        "uz_extract_plan_build(package_bank, work_bank, catalog_bank",
        "uz_extract_plan_count() != entry_count",
        "uz_dos_rename(UZWF_OUTPUT_DOS, workflow_temp, output_name)",
        "uz_dos_delete(UZWF_OUTPUT_DOS, workflow_temp)",
    )
    if all(needle in workflow_source for needle in recursive_create_needles):
        passed("recursive Create streams members, reopens to verify, and commits atomically")
    else:
        fail("recursive Create lacks streaming, reopen verification, or atomic cleanup")
        ok = False

    create_command_needles = (
        "UZ_CREATE_JOB_INPUT_DOS->command = UZ_CREATE_JOB_INPUT_COMMAND;",
        "UZ_CREATE_JOB_INPUT_DOS->command_cap =\n"
        "        UZ_CREATE_JOB_INPUT_COMMAND_CAP;",
        "UZWF_INPUT_DOS->command = UZWF_OUTPUT_COMMAND;",
        "UZWF_INPUT_DOS->command_cap =\n"
        "                UZ_CREATE_JOB_OUTPUT_COMMAND_CAP;",
    )
    if all(needle in create_job_source + "\n" + workflow_source
           for needle in create_command_needles):
        passed("Create separates live Deflate output from input command scratch")
    else:
        fail("Create input commands can overwrite buffered Deflate output")
        ok = False

    cleanup_needles = (
        "static unsigned char release_operation_banks",
        "reu_bank_type(catalog_bank) != REU_FREE",
        "reu_bank_type(work_bank) != REU_FREE",
        "find_current_resource_bank(REUCB_DEP_KIND_APP_ALLOC,\n"
        "                                   UZIP_CATALOG_SLOT)",
        "find_current_resource_bank(REUCB_DEP_KIND_APP_ALLOC,\n"
        "                                   UZIP_WORK_SLOT)",
        "cleanup_ok = release_operation_banks(work_bank, catalog_bank);",
        'if (!cleanup_ok) set_result_status("CREATE CLEANUP FAILED"',
        'else if (ok) set_result_status("CREATE COMPLETE"',
        'if (!cleanup_ok) set_result_status("EXTRACT CLEANUP FAILED"',
        'else if (ok) set_result_status("EXTRACT COMPLETE"',
    )
    if all(needle in app_source for needle in cleanup_needles):
        passed("production COMPLETE screens require transient REU cleanup proof")
    else:
        fail("production success can bypass transient REU cleanup verification")
        ok = False

    cold_start_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_cold_start.s"
    ).read_text(encoding="utf-8", errors="replace")
    warm_resume_needles = (
        "unsigned char uzip_ui_resume_marker[4];",
        "static unsigned char home_selected;",
        "int uzip_ui_warm_main(void)",
        "(void)open_preloaded_package();",
        "run_home();",
        "uzip_ui_resume_marker[0] = 0x55u",
        "uzip_ui_resume_marker[3] = 0x31u",
        ".import _uzip_ui_resume_marker",
        ".import _uz_cold_boot_run, _uzip_ui_main, _uzip_ui_warm_main",
        "jsr     _uzip_ui_warm_main",
        "cold_start:",
    )
    warm_resume_sources = app_source + "\n" + cold_start_source
    try:
        resume_marker = parse_symbol(MAP_PATH, "_uzip_ui_resume_marker")
    except ValueError as error:
        fail(str(error))
        ok = False
    else:
        ui_bss_start, ui_bss_end, _ = segments["UI_BSS"]
        if (all(needle in warm_resume_sources for needle in warm_resume_needles) and
                ui_bss_start <= resume_marker <= ui_bss_end - 3):
            passed("cold bridge distinguishes restored ReadyOS UI from first entry")
        else:
            fail("warm ReadyOS entry can destructively cold-initialize restored UI")
            ok = False

    workflow_plan_source = (
        ROOT / "build_support" / "build_uzip_workflow_plans.py"
    ).read_text(encoding="utf-8", errors="replace")
    workflow_runner_source = (
        ROOT / "build_support" / "run_uzip_workflow_c64u.sh"
    ).read_text(encoding="utf-8", errors="replace")
    workflow_analyzer_source = (
        ROOT / "build_support" / "analyze_uzip_workflow_outputs.py"
    ).read_text(encoding="utf-8", errors="replace")
    lifecycle_needles = (
        'wait("source_browser", "CREATE: MARK SOURCES"',
        'sequence("mark_file_and_folder", [32, 17, 32, 133]',
        'sequence("store_use_current_folder", [133])',
        'sequence("select_store_method", [145, 13])',
        'sequence("start_and_queue_cancel", [13, 3]',
        'wait("create_cancelled", "CREATE CANCELLED"',
        'wait("extract_complete", "EXTRACT COMPLETE"',
        'sequence("return_to_launcher", [3])',
        'wait("launcher_after_extract", "APPLICATIONS:"',
        'sequence("select_and_load_simplefiles", [145, 13], 0.0)',
        'wait("simplefiles_loaded", "SIMPLE FILES", app_load_wait_s,',
        'sequence("simplefiles_return_to_launcher", [2])',
        'wait("launcher_with_both_apps", "APPLICATIONS:"',
        'sequence("relaunch_loaded_uzip", [17, 13])',
        'wait("uzip_warm_result_preserved", "EXTRACT COMPLETE"',
        'speed="${UZIP_WORKFLOW_SPEED_MHZ:-16}"',
        '--speed-mhz "$speed"',
        'live CPU confirmed:',
        '"LOOSE.BIN:LOOSE.BIN"',
        '"TREE-DEEP.BIN:TREE/NEST/DEEP.BIN"',
        '"TREE/NEST/DEEP.BIN"',
        '--destination-list "$out_dir/destination-list.txt"',
        'echo "VICE use: forbidden"',
        "current-folder Store, safe cancel, recursive Deflate/Extract, and Python ZIP oracle passed",
    )
    lifecycle_sources = (workflow_plan_source + "\n" + workflow_runner_source +
                         "\n" + workflow_analyzer_source)
    if all(needle in lifecycle_sources for needle in lifecycle_needles):
        passed("recursive 16 MHz workflow pins launcher lifecycle and nested byte oracles")
    else:
        fail("recursive 16 MHz workflow lost a required physical C64U boundary")
        ok = False

    ui_create_needles = (
        "uz_browser_split_current(browser_path, source_path",
        '"F1 CURRENT FOLDER  F5 CLEAR"',
        "static unsigned char prompt_create_method(void)",
        'create_method == 0u ? "STORE" : "COMPRESS"',
        "extract_progress, 0",
        'set_result_status("CREATE CANCELLED", error, detail)',
        'set_result_status("EXTRACT CANCELLED", error, detail)',
        'wait("create_method", "ZIP METHOD", 60)',
    )
    ui_create_sources = app_source + "\n" + workflow_source + "\n" + workflow_plan_source
    if all(needle in ui_create_sources for needle in ui_create_needles):
        passed("production TUI offers current-folder Store/Compress and safe member cancellation")
    else:
        fail("production TUI lost current-folder, method, progress, or cancellation behavior")
        ok = False

    create_plan_source = (
        ROOT / "src" / "apps" / "uzip" / "uz_create_plan.c"
    ).read_text(encoding="utf-8", errors="replace")
    create_plan_diag = (
        ROOT / "src" / "apps" / "uzip" / "xuzcreateplan_diag.c"
    ).read_text(encoding="utf-8", errors="replace")
    create_plan_fixture = (
        ROOT / "build_support" / "build_xuzcreateplan_fixture.py"
    ).read_text(encoding="utf-8", errors="replace")
    create_plan_runner = (
        ROOT / "build_support" / "run_xuzcreateplan_c64u.sh"
    ).read_text(encoding="utf-8", errors="replace")
    create_plan_analyzer = (
        ROOT / "build_support" / "analyze_xuzcreateplan_run.py"
    ).read_text(encoding="utf-8", errors="replace")
    create_plan_needles = (
        "UZIP_CREATEPLAN_DIAGNOSTIC ?= 0",
        "for (index = 0u; index < plan->catalog->count; ++index)",
        "uz_catalog_append_unique(plan->catalog, plan->record",
        "UZ_CREATE_PLAN_ERR_OUTPUT_INSIDE",
        "Each callback fully drains one READ_DIR transaction",
        "uz_create_plan_seed(&plan, \"loose.prg\", 0u)",
        "uz_create_plan_seed(&plan, \"top\", 1u)",
        "uz_create_plan_build(&plan, XUZCREATEPLAN_OUTPUT_PATH)",
        '"entry_count": len(entries)',
        'if len(entries) != 23:',
        "run-ultimate-plan --plan",
        "Storage mutation by C64: none",
        "read-only source/output oracle passed",
        'raw.find(b"XZC1")',
        '"catalog_set_oracle": "pass"',
    )
    create_plan_sources = (make_source + "\n" + create_plan_source + "\n" +
                           create_plan_diag + "\n" + create_plan_fixture +
                           "\n" + create_plan_runner + "\n" +
                           create_plan_analyzer)
    if all(needle in create_plan_sources for needle in create_plan_needles):
        passed("recursive marked-source planner has host and physical C64U boundaries")
    else:
        fail("recursive create-plan physical probe contract is incomplete")
        ok = False

    dummy_source = (
        ROOT / "src" / "apps" / "uzip" / "xuzzip8_dummy_inflate.s"
    ).read_text(encoding="utf-8", errors="replace")
    if ("ifneq ($(filter 1,$(UZIP_SELF_SEED_SELECTION)),)" in make_source and
            "UZIP_INFLATE_SRCS = $(APPS_DIR)/uzip/xuzzip8_dummy_inflate.s" in
            make_source and '.segment "INFLATE_CODE"' in dummy_source):
        passed("unused dummy inflater is confined to focused diagnostics")
    else:
        fail("dummy inflater can leak into production or lacks linker segments")
        ok = False

    job_source = (ROOT / "src" / "apps" / "uzip" / "uz_job.c").read_text(
        encoding="utf-8", errors="replace"
    )
    diagnostic_ui_source = (
        ROOT / "src" / "apps" / "uzip" / "xuzdeflate_diag_ui.c"
    ).read_text(encoding="utf-8", errors="replace")
    mapping_needles = (
        "#define UZ_JOB_CPU_PORT",
        "coord_entry = (unsigned int)entry",
        "coord_entry < coord_run",
        "coord_entry >= coord_run + coord_size",
        "saved_cpu_port = UZ_JOB_CPU_PORT",
        "saved_cpu_port &",
        "~UZ_JOB_LORAM",
        "UZ_JOB_CPU_PORT = saved_cpu_port",
        "captured_inflate_error = uz_inflate_job_error();",
        "captured_inflate_codec_error = uz_inflate_job_codec_error();",
        "restore_ui_window();",
        "unsigned char uz_job_inflate_error(void)",
    )
    if (all(needle in job_source for needle in mapping_needles) and
            "work_bank,\n                                xuzdeflate_coord_entry" in
            diagnostic_ui_source):
        passed(
            "Deflate trampoline range-checks its explicit entry, exposes RAM "
            "under BASIC ROM, and restores $01"
        )
    else:
        fail("Deflate trampoline can execute the BASIC ROM instead of packed RAM")
        ok = False

    stack_source = (ROOT / "src" / "apps" / "uzip" / "xuzdeflate_stack.s").read_text(
        encoding="utf-8", errors="replace"
    )
    coord_source = (
        ROOT / "src" / "apps" / "uzip" / "xuzdeflate_diag_coord.c"
    ).read_text(encoding="utf-8", errors="replace")
    analyzer_source = (
        ROOT / "build_support" / "analyze_xuzdeflate_run.py"
    ).read_text(encoding="utf-8", errors="replace")
    stack_needles = (
        '.segment "DEFLATE_COORD_CODE"',
        "_xuzdeflate_stack_watermark_init",
        "_xuzdeflate_stack_watermark_low",
        "#<$C400",
        "#>$C400",
        "eor     #$5A",
    )
    if (all(needle in stack_source for needle in stack_needles) and
            "xuzdeflate_stack_watermark_init()" in coord_source and
            "xuzdeflate_stack_watermark_low()" in coord_source and
            "0xC400 <= stack_low <= stack_initial <= 0xC600" in analyzer_source):
        passed("physical compressor carries a coordinator-resident full-window stack watermark")
    else:
        fail("compressor stack proof no longer covers the complete ReadyOS window")
        ok = False

    compressor_plan = (
        ROOT / "build_support" / "xuzdeflate_ultimate.generated.yaml"
    ).read_text(encoding="utf-8", errors="replace")
    trace_needles = (
        "XUZD_TRACE_STAGE = XUZD_TRACE_PHASE_AFTER",
        "XUZD_TRACE_STAGE = XUZD_TRACE_WRITE_BEFORE",
        "trace=(${result[124]:02X}",
        "xuzdeflate_zp_0002",
        "xuzdeflate_state_0400",
        "xuzdeflate_coord_a000",
        "xuzdeflate_phase_b000",
        "xuzdeflate_stack_c400",
    )
    trace_sources = coord_source + "\n" + analyzer_source + "\n" + compressor_plan
    if all(needle in trace_sources for needle in trace_needles):
        passed("physical compressor timeout preserves phase/flush/stack evidence")
    else:
        fail("compressor timeout evidence no longer localizes overlay/stack failures")
        ok = False

    dos_header = (ROOT / "src" / "apps" / "uzip" / "uz_dos.h").read_text(
        encoding="utf-8", errors="replace"
    )
    dos_source = (ROOT / "src" / "apps" / "uzip" / "uz_dos.c").read_text(
        encoding="utf-8", errors="replace"
    )
    if re.search(r"#define\s+UZ_DOS_WRITE_MAX\s+508u", dos_header):
        passed("DOS writes retain the physically proven 508-byte payload cap")
    else:
        fail("DOS write payload cap changed without a new physical proof")
        ok = False
    ascii_needles = (
        "static unsigned char dos_ascii",
        "value == 0xA4u",
        "dos_ascii((unsigned char)*argument++)",
        "dos_ascii((unsigned char)*name++)",
    )
    if all(needle in dos_source for needle in ascii_needles):
        passed("DOS command arguments retain PETSCII-to-ASCII normalization")
    else:
        fail("DOS command-boundary ASCII normalization is incomplete")
        ok = False

    ultimate_ini = ULTIMATE_INI.read_text(encoding="utf-8", errors="replace")
    if "8:uzip:ultimate zip::uzpk\ncreate and extract ultimate dos zips" in ultimate_ini:
        passed("Ultimate launcher catalog labels uzip as Ultimate zip with uzpk preload")
    else:
        fail("Ultimate launcher catalog entry is missing or changed")
        ok = False
    if "8:ucitest:" not in ultimate_ini:
        passed("Ultimate release reserves D81 capacity by excluding the UCI lab app")
    else:
        fail("Ultimate catalog still carries ucitest and cannot fit the complete uZIP payload")
        ok = False

    expected_artifact = {
        "artifact": "uzip.prg",
        "name": "uzip",
        "type": "prg",
        "directory_group": "program",
    }
    expected_package = {
        "artifact": "uzpack.prg",
        "name": "uzpack",
        "type": "prg",
        "directory_group": "overlay",
    }
    ultimate_profile = json.loads(ULTIMATE_JSON.read_text(encoding="utf-8"))
    additions = ultimate_profile["disk_overrides"][0]["append_contents"]
    if expected_artifact in additions:
        passed("Ultimate release appends uzip.prg as an ordinary program")
    else:
        fail("Ultimate release does not append the expected uzip.prg artifact")
        ok = False
    if expected_package in additions:
        passed("Ultimate release appends uzpack.prg as an overlay resource")
    else:
        fail("Ultimate release does not append the expected uzpack.prg resource")
        ok = False

    leaked: list[str] = []
    for path in sorted((ROOT / "cfg" / "profiles").glob("precog-*")):
        if path in (ULTIMATE_INI, ULTIMATE_JSON) or not path.is_file():
            continue
        source = path.read_text(encoding="utf-8", errors="replace")
        if re.search(r"(?i)(?:^|[\"/:])uzip(?:[.\":]|$)", source, re.MULTILINE):
            leaked.append(path.name)
    if leaked:
        fail("uZIP leaked into non-Ultimate profiles: " + ", ".join(leaked))
        ok = False
    else:
        passed("uZIP is absent from every non-Ultimate profile")

    forbidden_automation = (
        "run-vice", "x64sc", "kind: vice_task_plan", "run_mode: vice"
    )
    automation_sources: list[Path] = []
    for probe_dir in (ROOT / "probes").glob("xuz*"):
        automation_sources.extend(probe_dir.glob("*"))
    automation_sources.extend((ROOT / "build_support").glob("*xuz*"))
    automation_errors: list[str] = []
    for path in automation_sources:
        if not path.is_file():
            continue
        source = path.read_text(encoding="utf-8", errors="replace").lower()
        for marker in forbidden_automation:
            if marker in source:
                automation_errors.append(f"{path.relative_to(ROOT)} contains {marker!r}")
    if automation_errors:
        fail("forbidden uZIP emulator automation: " + "; ".join(automation_errors))
        ok = False
    else:
        passed("uZIP probe/build automation contains no emulator launch target")

    print(
        f"uZIP windows: core $1000-$2FFF; idle UI $3000-$9FFF; "
        f"codec workspace $3000-$AFFF; "
        f"job $B000-$C3FF; stack $C400-$C5FF; shim starts $C600"
    )
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
