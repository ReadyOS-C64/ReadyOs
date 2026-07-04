#!/usr/bin/env python3
from __future__ import annotations

import os
import pathlib
import re
import sys


APP_LOAD = 0x1000
RS_LOAD = 0x8E00
APP_MAX = 0xB600
RS_MAX = 0x3800


def parse_profile(path: pathlib.Path) -> tuple[str, list[dict[str, object]]]:
    section = ""
    pending: dict[str, object] | None = None
    pending_desc = False
    image_name = os.environ.get("UCI_TIMING_IMAGE_NAME", "readyos.d81")
    c64u_path = f"/usb1/{image_name}"
    apps: list[dict[str, object]] = []

    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip().lower()
            continue
        if section == "launcher" and line.startswith("c64u_image_path="):
            c64u_path = line.split("=", 1)[1].strip() or c64u_path
            continue
        if section != "apps":
            continue
        if pending and pending_desc:
            for item in line.split(","):
                item = item.strip()
                if not item or "@" not in item or ":" not in item:
                    continue
                name, rest = item.split("@", 1)
                bank_raw, off_raw = rest.split(":", 1)
                apps.append({
                    "name": name.strip(),
                    "kind": "rs",
                    "bank": 0x80 + int(bank_raw, 10),
                    "off": int(off_raw, 16),
                    "max_len": RS_MAX,
                    "expected": RS_LOAD,
                })
            pending = None
            pending_desc = False
            continue
        if pending:
            resource = str(pending.get("resource") or "")
            if resource.endswith("+") and resource[:-1] == "rsovl":
                pending_desc = True
            else:
                pending = None
            continue
        if re.match(r"^[0-9]+:", line):
            parts = [p.strip() for p in line.split(":")]
            if len(parts) < 3:
                continue
            name = parts[1]
            resource = parts[4] if len(parts) >= 5 else (
                parts[3] if len(parts) >= 4 and not parts[3].isdigit() else ""
            )
            app = {
                "name": name,
                "kind": "app",
                "bank": 0x40 + sum(1 for a in apps if a.get("kind") == "app"),
                "off": 0,
                "max_len": APP_MAX,
                "expected": APP_LOAD,
                "resource": resource,
            }
            apps.append(app)
            pending = app

    return c64u_path, apps


def asm_string(text: str) -> str:
    return ", ".join(f"${b:02X}" for b in text.encode("ascii")) + ", 0"


def payload_len(root: pathlib.Path, name: str) -> int:
    path = root / "bin" / f"{name}.prg"
    data = path.read_bytes()
    if len(data) < 3:
        raise ValueError(f"{path}: too small")
    return len(data) - 2


def image_path_parts(profile_path: pathlib.Path) -> tuple[str, str]:
    c64u_path, _ = parse_profile(profile_path)
    image_name = os.environ.get("UCI_TIMING_IMAGE_NAME")
    if image_name:
        c64u_path = f"/usb1/{image_name}"
    slash = c64u_path.rfind("/")
    if slash <= 0 or slash == len(c64u_path) - 1:
        return "/", c64u_path.strip("/")
    return c64u_path[:slash], c64u_path[slash + 1:]


def build_workload(root: pathlib.Path, profile: pathlib.Path) -> tuple[str, int]:
    _, items = parse_profile(profile)
    only = [item.strip().lower() for item in os.environ.get("UCI_TIMING_ITEMS", "").split(",") if item.strip()]
    if only:
        wanted = set(only)
        items = [item for item in items if str(item["name"]).lower() in wanted]
        order = {name: i for i, name in enumerate(only)}
        items.sort(key=lambda item: order.get(str(item["name"]).lower(), 999))
    generated: list[str] = []
    labels: list[tuple[str, str]] = []
    used: set[str] = set()

    generated.append("workload_run:")
    for idx, item in enumerate(items):
        name = str(item["name"]).lower()
        label = "name_" + re.sub(r"[^a-z0-9_]", "_", name)
        if label in used:
            label = f"{label}_{idx}"
        used.add(label)
        labels.append((label, name))
        length = payload_len(root, name)
        raw_length = length + 2
        max_len = int(item["max_len"])
        if length > max_len and item["kind"] == "rs" and length <= 0x3A00:
            length = max_len
        if length > max_len:
            raise ValueError(f"{name}: payload ${length:04X} exceeds max ${max_len:04X}")
        probe_len = raw_length if os.environ.get("UCI_TIMING_FAST_RAW", "0") == "1" else max_len
        generated.append(f"""
        lda #<{label}
        sta current_name
        lda #>{label}
        sta current_name+1
        lda #${int(item['bank']) & 0xff:02X}
        sta current_bank
        lda #<${int(item['off']) & 0xffff:04X}
        sta current_off
        lda #>${int(item['off']) & 0xffff:04X}
        sta current_off+1
        lda #<${probe_len & 0xffff:04X}
        sta current_max
        lda #>${probe_len & 0xffff:04X}
        sta current_max+1
        lda #<${int(item['expected']) & 0xffff:04X}
        sta current_expected
        lda #>${int(item['expected']) & 0xffff:04X}
        sta current_expected+1
        jsr load_one_launcher_timed
        bcs @item_{idx}_ok
        inc failures
@item_{idx}_ok:
        inc item_index
        jsr update_runtime_results
""")

    generated.append("        rts\n")
    generated.append(f"WORKLOAD_COUNT = {len(items)}")
    for label, name in labels:
        generated.append(f"{label}: .byte " + asm_string(name))
    return "\n".join(generated), len(items)


ASM_TEMPLATE = r'''
        .segment "LOADADDR"
        .word $0801

        .segment "STARTUP"
        .word basic_next
        .word 10
        .byte $9e, "2061", 0
basic_next:
        .word 0

        .import _launcher_uci_dma_detect
        .import _launcher_uci_dma_load_prg
        .import _launcher_uci_dma_quiesce
        .import _launcher_uci_dma_clear_stage
        .import _launcher_uci_dma_name
        .import _launcher_uci_dma_reu_bank
        .import _launcher_uci_dma_reu_offset
        .import _launcher_uci_dma_max_len
        .import _launcher_uci_dma_expected_load_addr
        .import _launcher_uci_dma_loaded_size
        .import _launcher_uci_dma_last_error
        .import _launcher_uci_dma_dbg_stat0
        .import _launcher_uci_dma_dbg_stat1
        .import _launcher_uci_dma_image_dir
        .import _launcher_uci_dma_image_name
        .import _launcher_uci_dma_mount_name
        .import _launcher_uci_dma_assume_mounted

        .export _utime_stage_begin
        .export _utime_stage_end

CHROUT      = $FFD2
GETIN       = $FFE4
RESULTS     = $3000
STAGE_COUNT = $0D
current_name = $04

        .macro PRINT label
        lda #<label
        ldy #>label
        jsr print_z
        .endmacro

        .segment "CODE"

start:
        lda #$93
        jsr CHROUT
        lda #$01
        sta $0286
        jsr init_results
        PRINT title_msg
        jsr cr
        PRINT path_msg
        PRINT image_dir
        lda #'/'
        jsr CHROUT
        PRINT image_name
        jsr cr
        jsr _launcher_uci_dma_detect
        cmp #$00
        bne have_uci
        lda #$02
        sta done_code
        lda #WORKLOAD_COUNT
        sta failures
        jsr update_runtime_results
        PRINT no_uci_msg
        jsr cr
        jmp done
have_uci:
        PRINT uci_msg
        jsr cr
        jsr workload_run
        lda failures
        beq all_ok
        lda #$04
        bne set_done
all_ok:
        lda #$01
set_done:
        sta done_code
        jsr update_runtime_results
done:
        jsr print_summary
        jmp *

load_one_launcher_timed:
        jsr print_progress
        lda #$31
        sta $052d

        lda #<image_dir
        sta _launcher_uci_dma_image_dir
        lda #>image_dir
        sta _launcher_uci_dma_image_dir+1
        lda #<image_name
        sta _launcher_uci_dma_image_name
        sta _launcher_uci_dma_mount_name
        lda #>image_name
        sta _launcher_uci_dma_image_name+1
        sta _launcher_uci_dma_mount_name+1
        lda current_name
        sta _launcher_uci_dma_name
        lda current_name+1
        sta _launcher_uci_dma_name+1
        lda current_bank
        sta _launcher_uci_dma_reu_bank
        lda current_off
        sta _launcher_uci_dma_reu_offset
        lda current_off+1
        sta _launcher_uci_dma_reu_offset+1
        lda current_max
        sta _launcher_uci_dma_max_len
        lda current_max+1
        sta _launcher_uci_dma_max_len+1
        lda current_expected
        sta _launcher_uci_dma_expected_load_addr
        lda current_expected+1
        sta _launcher_uci_dma_expected_load_addr+1
        lda image_ready
        sta _launcher_uci_dma_assume_mounted

        jsr read_jiffy16
        sta tick_start
        stx tick_start+1
        jsr _launcher_uci_dma_load_prg
        sta last_ok
        tax
        beq load_failed_quiesce
        lda #$01
        sta image_ready
        sta _launcher_uci_dma_assume_mounted
        jsr _launcher_uci_dma_quiesce
        jsr _launcher_uci_dma_clear_stage
        jmp after_loader_call
load_failed_quiesce:
        jsr apply_launcher_failure_state
        jsr _launcher_uci_dma_quiesce
after_loader_call:
        jsr read_jiffy16
        sec
        sbc tick_start
        sta last_ticks
        txa
        sbc tick_start+1
        sta last_ticks+1
        jsr add_total_ticks
        jsr add_phase_ticks
        jsr update_max_load
        lda last_ok
        bne load_succeeded
        jsr store_first_failure
        clc
        rts
load_succeeded:
        inc successes
        jsr add_loaded_kb
        sec
        rts

apply_launcher_failure_state:
        lda _launcher_uci_dma_last_error
        cmp #$01
        beq fail_disable_path
        cmp #$08
        bcc fail_keep_path
        cmp #$11
        bcs fail_keep_path
fail_disable_path:
        lda #$00
        sta image_ready
        sta _launcher_uci_dma_assume_mounted
        rts
fail_keep_path:
        lda #$01
        sta image_ready
        sta _launcher_uci_dma_assume_mounted
        rts

add_total_ticks:
        lda total_ticks
        clc
        adc last_ticks
        sta total_ticks
        lda total_ticks+1
        adc last_ticks+1
        sta total_ticks+1
        rts

add_phase_ticks:
        lda item_index
        beq add_first_ticks
        lda subsequent_ticks
        clc
        adc last_ticks
        sta subsequent_ticks
        lda subsequent_ticks+1
        adc last_ticks+1
        sta subsequent_ticks+1
        rts
add_first_ticks:
        lda first_ticks
        clc
        adc last_ticks
        sta first_ticks
        lda first_ticks+1
        adc last_ticks+1
        sta first_ticks+1
        rts

update_max_load:
        lda max_load_ticks+1
        cmp last_ticks+1
        bcc set_max_load
        bne max_load_done
        lda max_load_ticks
        cmp last_ticks
        bcs max_load_done
set_max_load:
        lda last_ticks
        sta max_load_ticks
        lda last_ticks+1
        sta max_load_ticks+1
        ldy #$00
copy_max_name:
        cpy #$0f
        beq term_max_name
        lda (current_name),y
        sta max_load_name,y
        beq max_load_done
        iny
        bne copy_max_name
term_max_name:
        lda #$00
        sta max_load_name,y
max_load_done:
        rts

add_loaded_kb:
        lda _launcher_uci_dma_loaded_size
        clc
        adc #$ff
        lda _launcher_uci_dma_loaded_size+1
        adc #$03
        lsr
        lsr
        clc
        adc total_kb
        sta total_kb
        lda total_kb+1
        adc #$00
        sta total_kb+1
        rts

store_first_failure:
        lda first_fail_error
        bne first_failure_done
        lda item_index
        sta first_fail_item
        lda _launcher_uci_dma_last_error
        sta first_fail_error
        lda _launcher_uci_dma_dbg_stat0
        sta first_fail_dbg0
        lda _launcher_uci_dma_dbg_stat1
        sta first_fail_dbg1
        ldy #$00
copy_fail_name:
        cpy #$0f
        beq term_fail_name
        lda (current_name),y
        sta first_fail_name,y
        beq first_failure_done
        iny
        bne copy_fail_name
term_fail_name:
        lda #$00
        sta first_fail_name,y
first_failure_done:
        rts

read_jiffy16:
        lda $A2
        ldx $A1
        rts

_utime_stage_begin:
        sta stage_id
        jsr read_jiffy16
        sta stage_start
        stx stage_start+1
        rts

_utime_stage_end:
        jsr read_jiffy16
        sta stage_now
        stx stage_now+1
        lda stage_now
        sec
        sbc stage_start
        sta stage_delta
        lda stage_now+1
        sbc stage_start+1
        sta stage_delta+1
        lda stage_id
        asl
        tax
        lda stage_ticks,x
        clc
        adc stage_delta
        sta stage_ticks,x
        lda stage_ticks+1,x
        adc stage_delta+1
        sta stage_ticks+1,x
        lda item_index
        beq stage_end_done
        lda stage_id
        asl
        tax
        lda stage_subseq_ticks,x
        clc
        adc stage_delta
        sta stage_subseq_ticks,x
        lda stage_subseq_ticks+1,x
        adc stage_delta+1
        sta stage_subseq_ticks+1,x
stage_end_done:
        rts

init_results:
        ldx #$00
        lda #$00
clear_results:
        sta RESULTS,x
        inx
        cpx #$80
        bne clear_results
        lda #'U'
        sta RESULTS
        lda #'T'
        sta RESULTS+1
        lda #'I'
        sta RESULTS+2
        lda #'M'
        sta RESULTS+3
        lda #__RESULT_VERSION__
        sta RESULTS+4
        lda #WORKLOAD_COUNT
        sta RESULTS+6
        jsr update_runtime_results
        rts

update_runtime_results:
        lda #__RESULT_VERSION__
        sta RESULTS+4
        lda done_code
        sta RESULTS+5
        lda item_index
        sta RESULTS+6
        lda failures
        sta RESULTS+7
        lda total_ticks
        sta RESULTS+8
        lda total_ticks+1
        sta RESULTS+9
        lda total_kb
        sta RESULTS+10
        lda total_kb+1
        sta RESULTS+11
        lda max_load_ticks
        sta RESULTS+12
        lda max_load_ticks+1
        sta RESULTS+13
        lda successes
        sta RESULTS+14
        lda #STAGE_COUNT
        sta RESULTS+15
        lda first_ticks
        sta RESULTS+16
        lda first_ticks+1
        sta RESULTS+17
        lda subsequent_ticks
        sta RESULTS+18
        lda subsequent_ticks+1
        sta RESULTS+19
        ldx #$00
copy_stages_result:
        lda stage_ticks,x
        sta RESULTS+20,x
        inx
        cpx #(STAGE_COUNT * 2)
        bne copy_stages_result
        ldx #$00
copy_subseq_stages_result:
        lda stage_subseq_ticks,x
        sta RESULTS+80,x
        inx
        cpx #(STAGE_COUNT * 2)
        bne copy_subseq_stages_result
        ldx #$00
copy_max_result:
        lda max_load_name,x
        sta RESULTS+48,x
        inx
        cpx #$10
        bne copy_max_result
        lda first_fail_item
        sta RESULTS+64
        lda first_fail_error
        sta RESULTS+65
        lda first_fail_dbg0
        sta RESULTS+66
        lda first_fail_dbg1
        sta RESULTS+67
        ldx #$00
copy_fail_result:
        lda first_fail_name,x
        sta RESULTS+72,x
        inx
        cpx #$08
        bne copy_fail_result
        rts

print_progress:
        lda #$13
        jsr CHROUT
        PRINT loading_msg
        lda item_index
        jsr print_hex_a
        lda #'/'
        jsr CHROUT
        lda #WORKLOAD_COUNT
        jsr print_hex_a
        lda #' '
        jsr CHROUT
        ldy #$00
progress_name:
        lda (current_name),y
        beq progress_done
        jsr CHROUT
        iny
        cpy #$0f
        bne progress_name
progress_done:
        jsr cr
        rts

print_summary:
        lda #$13
        jsr CHROUT
        PRINT done_msg
        lda done_code
        jsr print_hex_a
        PRINT item_msg
        lda item_index
        jsr print_hex_a
        PRINT fail_msg
        lda failures
        jsr print_hex_a
        jsr cr
        PRINT succ_msg
        lda successes
        jsr print_hex_a
        PRINT kb_msg
        lda total_kb+1
        jsr print_hex_a
        lda total_kb
        jsr print_hex_a
        PRINT ticks_msg
        lda total_ticks+1
        jsr print_hex_a
        lda total_ticks
        jsr print_hex_a
        jsr cr
        PRINT max_msg
        lda max_load_ticks+1
        jsr print_hex_a
        lda max_load_ticks
        jsr print_hex_a
        lda #' '
        jsr CHROUT
        lda #<max_load_name
        ldy #>max_load_name
        jsr print_z
        jsr cr
        lda failures
        beq no_fail_print
        PRINT first_fail_msg
        lda first_fail_item
        jsr print_hex_a
        lda #'/'
        jsr CHROUT
        lda first_fail_error
        jsr print_hex_a
        lda #' '
        jsr CHROUT
        lda #<first_fail_name
        ldy #>first_fail_name
        jsr print_z
        jsr cr
no_fail_print:
        PRINT probe_done_msg
        jsr cr
        rts

print_z:
        sta pz+1
        sty pz+2
        ldy #$00
pz:
        lda $FFFF,y
        beq print_z_done
        jsr CHROUT
        iny
        bne pz
print_z_done:
        rts

print_hex_a:
        pha
        lsr
        lsr
        lsr
        lsr
        jsr print_hex_nibble
        pla
        and #$0f
print_hex_nibble:
        cmp #$0a
        bcc print_hex_digit
        adc #$06
print_hex_digit:
        adc #'0'
        jsr CHROUT
        rts

cr:
        lda #$0d
        jsr CHROUT
        rts

        .segment "RODATA"
__WORKLOAD__
image_dir: .byte __IMAGE_DIR__
image_name: .byte __IMAGE_NAME__
title_msg: .byte __TITLE_MSG__
path_msg: .byte "PATH ", 0
uci_msg: .byte "UCI OK", 0
no_uci_msg: .byte "NO UCI", 0
loading_msg: .byte "LOADING ", 0
done_msg: .byte "DONE:", 0
item_msg: .byte " ITEMS:", 0
fail_msg: .byte " FAIL:", 0
succ_msg: .byte "SUCCESS:", 0
kb_msg: .byte " KB:", 0
ticks_msg: .byte " TICKS:", 0
max_msg: .byte "MAX:", 0
first_fail_msg: .byte "FIRST FAIL:", 0
probe_done_msg: .byte "PROBE DONE", 0

        .segment "DATA"
done_code:       .byte 0
item_index:      .byte 0
successes:       .byte 0
failures:        .byte 0
image_ready:     .byte 0
last_ok:         .byte 0
tick_start:      .word 0
last_ticks:      .word 0
total_ticks:     .word 0
first_ticks:     .word 0
subsequent_ticks:.word 0
total_kb:        .word 0
max_load_ticks:  .word 0
current_bank:    .byte 0
current_off:     .word 0
current_max:     .word 0
current_expected:.word 0
first_fail_item: .byte 0
first_fail_error:.byte 0
first_fail_dbg0: .byte 0
first_fail_dbg1: .byte 0
max_load_name:   .res 16
first_fail_name: .res 16
stage_id:        .byte 0
stage_start:     .word 0
stage_now:       .word 0
stage_delta:     .word 0
stage_ticks:     .res STAGE_COUNT * 2
stage_subseq_ticks: .res STAGE_COUNT * 2
'''


def main() -> int:
    if len(sys.argv) != 5:
        raise SystemExit("usage: build_asm_probe.py ROOT TEMPLATE PROFILE OUTPUT")
    root = pathlib.Path(sys.argv[1]).resolve()
    profile = pathlib.Path(sys.argv[3]).resolve()
    output = pathlib.Path(sys.argv[4]).resolve()

    workload, _ = build_workload(root, profile)
    image_dir, image_name = image_path_parts(profile)
    source = ASM_TEMPLATE.replace("__WORKLOAD__", workload)
    source = source.replace("__IMAGE_DIR__", asm_string(image_dir.lower()))
    source = source.replace("__IMAGE_NAME__", asm_string(image_name.lower()))
    if os.environ.get("UCI_TIMING_FAST_RAW", "0") == "1":
        source = source.replace("__RESULT_VERSION__", "$06")
        source = source.replace("__TITLE_MSG__", asm_string("UCI TIMING FAST RAW"))
    else:
        source = source.replace("__RESULT_VERSION__", "$04")
        source = source.replace("__TITLE_MSG__", asm_string("UCI TIMING LAUNCHER"))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(source, encoding="utf-8")
    print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
