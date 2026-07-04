#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import sys
import os


REPLACEMENTS = [
    ("jsr _launcher_uci_dma_detect", "jsr utime_call_detect"),
    ("jsr dos_identify\n        BCC_FAR load_no_uci_fail",
     "jsr utime_call_identify\n        BCC_FAR load_no_uci_fail"),
    ("jsr ctrl_get_drvinfo\n        BCC_FAR load_no_uci_fail",
     "jsr utime_call_drvinfo\n        BCC_FAR load_no_uci_fail"),
    ("jsr dos_open_read\n        BCC_FAR load_open_fail",
     "jsr utime_call_open\n        BCC_FAR load_open_fail"),
    ("jsr dos_file_info\n        BCC_FAR load_stat_fail",
     "jsr utime_call_info\n        BCC_FAR load_stat_fail"),
    ("jsr dos_read_header\n        BCC_FAR load_header_fail",
     "jsr utime_call_read_header\n        BCC_FAR load_header_fail"),
    ("jsr dos_seek_payload\n        BCC_FAR load_seek_fail",
     "jsr utime_call_seek\n        BCC_FAR load_seek_fail"),
    ("jsr dos_load_reu_chunk\n        BCC_FAR load_load_fail",
     "jsr utime_call_load_reu\n        BCC_FAR load_load_fail"),
    ("jsr dos_close\n        lda #$00\n        sta _launcher_uci_dma_last_error",
     "jsr utime_call_close\n        lda #$00\n        sta _launcher_uci_dma_last_error"),
]


TARGETED_CD_REPLACEMENTS = [
    ("jsr dos_cd\n        BCC_FAR load_cd_root_fail", "jsr utime_call_cd_root\n        BCC_FAR load_cd_root_fail"),
    ("jsr dos_cd\n        BCC_FAR load_cd_dir_fail", "jsr utime_call_cd_dir\n        BCC_FAR load_cd_dir_fail"),
    ("jsr dos_cd\n        BCC_FAR load_cd_image_fail", "jsr utime_call_cd_image\n        BCC_FAR load_cd_image_fail"),
]


WRAPPERS = r'''

; Probe-only instrumentation wrappers. These preserve the carry flag returned
; by the underlying launcher UCI call so the production control flow is intact.
        .import _utime_stage_begin
        .import _utime_stage_end

utime_call_detect:
        lda #$00
        jsr _utime_stage_begin
        jsr _launcher_uci_dma_detect
        php
        lda #$00
        jsr _utime_stage_end
        plp
        rts

utime_call_identify:
        lda #$01
        jsr _utime_stage_begin
        jsr dos_identify
        php
        lda #$01
        jsr _utime_stage_end
        plp
        rts

utime_call_drvinfo:
        lda #$02
        jsr _utime_stage_begin
        jsr ctrl_get_drvinfo
        php
        lda #$02
        jsr _utime_stage_end
        plp
        rts

utime_call_cd_root:
        lda #$03
        jsr _utime_stage_begin
        jsr dos_cd
        php
        lda #$03
        jsr _utime_stage_end
        plp
        rts

utime_call_cd_dir:
        lda #$04
        jsr _utime_stage_begin
        jsr dos_cd
        php
        lda #$04
        jsr _utime_stage_end
        plp
        rts

utime_call_mount:
        lda #$05
        jsr _utime_stage_begin
        jsr dos_mount_image
        php
        lda #$05
        jsr _utime_stage_end
        plp
        rts

utime_call_cd_image:
        lda #$06
        jsr _utime_stage_begin
        jsr dos_cd
        php
        lda #$06
        jsr _utime_stage_end
        plp
        rts

utime_call_open:
        lda #$07
        jsr _utime_stage_begin
        jsr dos_open_read
        php
        lda #$07
        jsr _utime_stage_end
        plp
        rts

utime_call_info:
        lda #$08
        jsr _utime_stage_begin
        jsr dos_file_info
        php
        lda #$08
        jsr _utime_stage_end
        plp
        rts

utime_call_read_header:
        lda #$09
        jsr _utime_stage_begin
        jsr dos_read_header
        php
        lda #$09
        jsr _utime_stage_end
        plp
        rts

utime_call_seek:
        lda #$0A
        jsr _utime_stage_begin
        jsr dos_seek_payload
        php
        lda #$0A
        jsr _utime_stage_end
        plp
        rts

utime_call_load_reu:
        lda #$0B
        jsr _utime_stage_begin
        jsr dos_load_reu_chunk
        php
        lda #$0B
        jsr _utime_stage_end
        plp
        rts

utime_call_close:
        lda #$0C
        jsr _utime_stage_begin
        jsr dos_close
        php
        lda #$0C
        jsr _utime_stage_end
        plp
        rts
'''


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: instrument_launcher_uci_dma_for_timing.py INPUT OUTPUT")
    src = pathlib.Path(sys.argv[1]).resolve()
    out = pathlib.Path(sys.argv[2]).resolve()
    text = src.read_text(encoding="utf-8")

    load_start = text.index("_launcher_uci_dma_load_prg:")
    load_end = text.index("dos_identify:")
    before = text[:load_start]
    body = text[load_start:load_end]
    after = text[load_end:]
    fast_raw = os.environ.get("UCI_TIMING_FAST_RAW", "0") == "1"

    if fast_raw:
        hot_skip_from = """        jsr _launcher_uci_dma_detect
        bne load_have_uci"""
        hot_skip_to = """        lda _launcher_uci_dma_assume_mounted
        beq load_regular_detect
        lda #'5'
        jsr debug_stage
        jmp load_open_current_dir
load_regular_detect:
        jsr _launcher_uci_dma_detect
        bne load_have_uci"""
        if hot_skip_from not in body:
            raise SystemExit("missing detect hot-skip insertion point")
        body = body.replace(hot_skip_from, hot_skip_to, 1)

        open_start = body.index("load_open_current_dir:")
        open_end = body.index("load_success:")
        fast_block = """load_open_current_dir:
        jsr utime_call_open
        BCC_FAR load_open_fail
        jsr status_ok
        BCC_FAR load_open_fail
        lda #'7'
        jsr debug_stage

        lda _launcher_uci_dma_reu_offset
        sta current_off_lo
        lda _launcher_uci_dma_reu_offset+1
        sta current_off_hi
        lda _launcher_uci_dma_max_len
        sta chunk_lo
        sta _launcher_uci_dma_loaded_size
        lda _launcher_uci_dma_max_len+1
        sta chunk_hi
        sta _launcher_uci_dma_loaded_size+1
        lda chunk_lo
        ora chunk_hi
        BEQ_FAR load_size_fail

        lda #'9'
        jsr debug_stage
        jsr utime_call_load_reu
        BCC_FAR load_load_fail
        jsr status_ok
        BCC_FAR load_load_fail
        jmp load_success

"""
        body = body[:open_start] + fast_block + body[open_end:]

    replacements = REPLACEMENTS
    if fast_raw:
        replacements = [
            item for item in REPLACEMENTS
            if "dos_file_info" not in item[0]
            and "dos_read_header" not in item[0]
            and "dos_seek_payload" not in item[0]
            and "dos_load_reu_chunk" not in item[0]
            and "dos_open_read" not in item[0]
        ]
    for old, new in replacements:
        if old not in body:
            raise SystemExit(f"missing call pattern: {old!r}")
        body = body.replace(old, new, 1)
    if "jsr dos_mount_image\n        BCC_FAR load_mount_fail" not in body:
        raise SystemExit("missing mount call pattern")
    body = body.replace("jsr dos_mount_image\n        BCC_FAR load_mount_fail",
                        "jsr utime_call_mount\n        BCC_FAR load_mount_fail", 1)
    for old, new in TARGETED_CD_REPLACEMENTS:
        if old not in body:
            raise SystemExit(f"missing cd pattern: {old!r}")
        body = body.replace(old, new)

    marker = '        .segment "RODATA"\n'
    if marker not in after:
        raise SystemExit("missing RODATA marker")
    after = after.replace(marker, WRAPPERS + "\n" + marker, 1)

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(before + body + after, encoding="utf-8")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
