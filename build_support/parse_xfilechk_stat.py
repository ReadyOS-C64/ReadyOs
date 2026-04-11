#!/usr/bin/env python3
"""Parse xfilechk binary result block."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


STEP_NAMES = {
    0: "none",
    1: "mode",
    2: "probe",
    3: "scratch",
    4: "copy_cmd",
    5: "copy_io",
    6: "verify",
    7: "dump",
    8: "stage",
}


def bit_drives(mask: int) -> str:
    parts = []
    for bit, drive in enumerate((8, 9, 10, 11)):
        if mask & (1 << bit):
            parts.append(str(drive))
    return ",".join(parts) if parts else "-"


def u16(lo: int, hi: int) -> int:
    return lo | (hi << 8)


def decode_field(raw: bytes) -> str:
    text = raw.replace(b"\xa0", b" ").replace(b"\x00", b" ")
    return text.decode("latin1", "replace").strip()


def probe_slot(raw: bytes, base: int) -> dict[str, object]:
    return {
        "raw_open_rc": raw[base + 0],
        "raw_read_rc": raw[base + 1],
        "raw_status_rc": raw[base + 2],
        "raw_status_code": raw[base + 3],
        "bam_data_open_rc": raw[base + 4],
        "bam_cmd_open_rc": raw[base + 5],
        "bam_status_rc": raw[base + 6],
        "bam_status_code": raw[base + 7],
        "bam_read_len": u16(raw[base + 8], raw[base + 9]),
        "raw_preview": decode_field(raw[base + 10:base + 26]),
        "raw_name": decode_field(raw[base + 26:base + 42]),
        "raw_id": decode_field(raw[base + 42:base + 46]),
        "bam_name_field": decode_field(raw[base + 46:base + 62]),
        "bam_id_field": decode_field(raw[base + 62:base + 66]),
        "bam_name": decode_field(raw[base + 66:base + 82]),
        "bam_id": decode_field(raw[base + 82:base + 86]),
    }


def bam_free_blocks(path: str) -> int:
    raw = Path(path).read_bytes()
    if len(raw) != 349696:
        raise ValueError(f"unexpected D71 size for {path}: {len(raw)}")

    side1 = sum(raw[0x16504 + (track - 1) * 4] for track in range(1, 36))
    side2 = sum(raw[0x16504 + 0xDD + (track - 36)] for track in range(36, 71))
    return side1 + side2


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dbg-bin", required=True)
    ap.add_argument("--disk8")
    ap.add_argument("--disk9")
    args = ap.parse_args()

    with open(args.dbg_bin, "rb") as f:
        raw = f.read()

    if len(raw) < 0x80:
        print(f"error: short xfilechk status block ({len(raw)} bytes)", file=sys.stderr)
        return 2

    marker = raw[0]
    version = raw[1]
    ok = raw[2]
    case_id = raw[3]
    fail_step = raw[4]
    fail_detail = raw[5]
    mode8 = raw[6]
    mode9 = raw[7]
    op_a = raw[0x0C]
    op_b = raw[0x0D]
    src_type = raw[0x10]
    src_size = u16(raw[0x11], raw[0x12])
    dst_type = raw[0x13]
    dst_size = u16(raw[0x14], raw[0x15])
    src_len = raw[0x16]
    dst_len = raw[0x17]
    scratch_rc = raw[0x18]
    scratch_status = raw[0x19]
    copy_rc = raw[0x1A]
    copy_status = raw[0x1B]
    verify_code = raw[0x1C]
    dump_open = raw[0x1D]
    dump_write = raw[0x1E]
    probe_cmd = raw[0x20]
    probe_open = raw[0x21]
    probe_first = raw[0x22]
    probe_recovery = raw[0x23]
    free8 = u16(raw[0x25], raw[0x26])
    free9 = u16(raw[0x27], raw[0x28])
    stages = bytes(b for b in raw[0x30:0x30 + raw[0x3F]] if b).decode("latin1", "replace")
    status_msg = bytes(b for b in raw[0x40:0x54] if b).decode("latin1", "replace")

    print(
        f"marker=0x{marker:02X} version={version} case={case_id} ok={ok} "
        f"fail_step={fail_step}({STEP_NAMES.get(fail_step, 'unknown')}) fail_detail={fail_detail}"
    )
    print(f"mode_rc: d8={mode8} d9={mode9}")
    if case_id in (11, 12, 13):
        print(
            f"probe case: base8={probe_cmd} base9={probe_open} "
            f"touch={probe_first} rec8={probe_recovery} rec9={raw[0x24]}"
        )
    else:
        print(
            f"probe cmd={probe_cmd:02X}[{bit_drives(probe_cmd)}] "
            f"open={probe_open:02X}[{bit_drives(probe_open)}] "
            f"first={probe_first:02X}[{bit_drives(probe_first)}] "
            f"recovery={probe_recovery:02X}[{bit_drives(probe_recovery)}]"
        )
    print(
        f"op_fields: a={op_a} b={op_b} scratch_rc={scratch_rc} scratch_st={scratch_status} "
        f"copy_rc={copy_rc} copy_st={copy_status} verify={verify_code}"
    )
    print(
        f"src: type={src_type} size={src_size} len={src_len} "
        f"dst: type={dst_type} size={dst_size} len={dst_len}"
    )
    if case_id == 14:
        print(f"free_blocks: d8={free8} d9={free9}")
        if args.disk8:
            exp8 = bam_free_blocks(args.disk8)
            print(f"free_blocks_bam: d8={exp8}")
            if free8 != exp8:
                print("RESULT: FAIL (drive8 free blocks mismatch)")
                return 1
        if args.disk9:
            exp9 = bam_free_blocks(args.disk9)
            print(f"free_blocks_bam: d9={exp9}")
            if free9 != exp9:
                print("RESULT: FAIL (drive9 free blocks mismatch)")
                return 1
    if case_id == 15:
        d8 = probe_slot(raw, 0x80)
        d9 = probe_slot(raw, 0xE0)
        print(
            "drvi d8:"
            f" raw(open={d8['raw_open_rc']} read={d8['raw_read_rc']} st_rc={d8['raw_status_rc']} st={d8['raw_status_code']}"
            f" preview={d8['raw_preview']!r} name={d8['raw_name']!r} id={d8['raw_id']!r})"
        )
        print(
            "drvi d8:"
            f" bam(data_open={d8['bam_data_open_rc']} cmd_open={d8['bam_cmd_open_rc']} st_rc={d8['bam_status_rc']}"
            f" st={d8['bam_status_code']} read_len={d8['bam_read_len']}"
            f" field_name={d8['bam_name_field']!r} field_id={d8['bam_id_field']!r}"
            f" name={d8['bam_name']!r} id={d8['bam_id']!r})"
        )
        print(
            "drvi d9:"
            f" raw(open={d9['raw_open_rc']} read={d9['raw_read_rc']} st_rc={d9['raw_status_rc']} st={d9['raw_status_code']}"
            f" preview={d9['raw_preview']!r} name={d9['raw_name']!r} id={d9['raw_id']!r})"
        )
        print(
            "drvi d9:"
            f" bam(data_open={d9['bam_data_open_rc']} cmd_open={d9['bam_cmd_open_rc']} st_rc={d9['bam_status_rc']}"
            f" st={d9['bam_status_code']} read_len={d9['bam_read_len']}"
            f" field_name={d9['bam_name_field']!r} field_id={d9['bam_id_field']!r}"
            f" name={d9['bam_name']!r} id={d9['bam_id']!r})"
        )
        if d8["raw_preview"].upper() != "XFILECHK8":
            print("RESULT: FAIL (drive8 directory header mismatch)")
            return 1
        if d9["raw_preview"].upper() != "XFILECHK9":
            print("RESULT: FAIL (drive9 directory header mismatch)")
            return 1
    print(f"stages={stages or '-'} status_msg={status_msg or '-'} dump_open={dump_open} dump_write={dump_write}")

    if marker != 0x58 or version != 0x01:
        print("RESULT: FAIL (bad marker/version)")
        return 1
    if dump_open not in (0, 255):
        print("RESULT: FAIL (status dump open failed)")
        return 1
    if ok != 1:
        print("RESULT: FAIL")
        return 1

    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
