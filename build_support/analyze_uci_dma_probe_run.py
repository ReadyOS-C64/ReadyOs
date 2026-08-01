#!/usr/bin/env python3
import argparse
import json
import pathlib
import sys


EXPECTED_VERSION = 0x20
FILE_BLOCKS = (
    (0x10, 0x11, "reu_snap_udma1_6000"),
    (0x20, 0x42, "reu_snap_udma2_6100"),
    (0x30, 0x83, "reu_snap_udma3_6200"),
)
PLAIN_STAT_OFFSET = 0x40
COPY_UI_STAT_OFFSET = 0x48


def fail(message):
    print(f"FAIL: {message}")
    return 1


def read_file(path):
    try:
        return path.read_bytes()
    except FileNotFoundError:
        return None


def artifact_path(run_dir, label):
    direct_capture = run_dir / f"{label}.bin"
    if direct_capture.exists():
        return direct_capture

    direct = run_dir / "stages" / "dump_probe_state" / f"{label}.bin"
    if direct.exists():
        return direct

    manifest_path = run_dir / "manifest.json"
    if not manifest_path.exists():
        return direct
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    except Exception:
        return direct
    for step in manifest.get("steps", []):
        artifacts = step.get("artifacts", {})
        value = artifacts.get(f"{label}.bin")
        if value:
            return pathlib.Path(value)
    return direct


def validate_result_header(result, expected_version):
    if result is None:
        return "missing probe_results_3000.bin"
    if len(result) < 0x40:
        return f"short probe result block ({len(result)} bytes)"
    if result[0:4] != b"UDMA":
        return "probe result magic is not UDMA"
    if result[10] != expected_version:
        return f"version ${result[10]:02X}, expected ${expected_version:02X}"
    return None


def validate_no_uci(result):
    if result[4] != 0:
        return f"expected no UCI, but UCI flag is ${result[4]:02X}"
    for offset, _, _ in FILE_BLOCKS:
        if result[offset] != 0:
            return f"file block ${offset:02X} changed on no-UCI path: ${result[offset]:02X}"
    return None


def validate_success(result, ram, reu, snapshots):
    if result[4] != 1:
        return f"UCI flag is ${result[4]:02X}, expected $01"
    if result[9] in (0x00, 0xFF):
        return f"SoftIEC bus byte ${result[9]:02X}, expected a real bus id"
    if result[0x0B] not in (0, 1):
        return f"selected USB byte is invalid: ${result[0x0B]:02X}"

    for index, (offset, pattern, label) in enumerate(FILE_BLOCKS, start=1):
        status = result[offset]
        if status != 0x55:
            return f"file {index} status ${status:02X}, expected $55"
        if result[offset + 4] != 1:
            return f"file {index} RAM flag ${result[offset + 4]:02X}, expected $01"
        if result[offset + 5] != 1:
            return f"file {index} REU flag ${result[offset + 5]:02X}, expected $01"
        if result[offset + 6] not in (1, 4):
            return f"file {index} RAM batch count ${result[offset + 6]:02X}, expected $01 or $04"
        size = result[offset + 7] | (result[offset + 8] << 8)
        if size not in (0x0000, 0x0102):
            return f"file {index} stat size ${size:04X}, expected $0000 or $0102"
        snapshot_hi = result[offset + 9]
        expected_hi = 0x5F + index
        if snapshot_hi != expected_hi:
            return (
                f"file {index} snapshot page ${snapshot_hi:02X}, "
                f"expected ${expected_hi:02X}"
            )
        snapshot = snapshots.get(label)
        if snapshot is None:
            return f"missing {label}.bin"
        if len(snapshot) < 0x100:
            return f"short {label} dump ({len(snapshot)} bytes)"
        if any(byte != pattern for byte in snapshot[:0x100]):
            return f"{label} does not contain expected pattern ${pattern:02X}"

    if ram is None:
        return "missing dma_load_buf_4000.bin"
    if len(ram) < 0x100:
        return f"short RAM dump ({len(ram)} bytes)"
    if any(byte != 0x83 for byte in ram[:0x100]):
        return "RAM dump does not contain final UDMA3 pattern $83"

    if reu is None:
        return "missing reu_fetch_buf_5000.bin"
    if len(reu) < 0x100:
        return f"short REU fetch dump ({len(reu)} bytes)"
    if any(byte != 0x83 for byte in reu[:0x100]):
        return "REU fetch dump does not contain final UDMA3 pattern $83"
    return None


def describe_context_stat(result, offset, name, labels):
    if result is None or len(result) <= offset + 7:
        return f"{name} unavailable"
    status = result[offset]
    return (
        f"{name} {labels.get(status, 'unknown')} "
        f"status=${status:02X} data_len=${result[offset + 1]:02X} "
        f"stat_len=${result[offset + 2]:02X} "
        f"stat=${result[offset + 3]:02X}${result[offset + 4]:02X} "
        f"data0=${result[offset + 5]:02X}${result[offset + 6]:02X} "
        f"data8=${result[offset + 7]:02X}"
    )


def describe_plain_stat(result):
    labels = {
        0x00: "not-run",
        0x41: "transport-fail",
        0x42: "not-file-stat",
        0x55: "ok",
    }
    return describe_context_stat(result, PLAIN_STAT_OFFSET, "plain-stat", labels)


def describe_copy_ui_stat(result):
    labels = {
        0x00: "not-run",
        0x51: "copy-ui-fail",
        0x52: "transport-fail",
        0x53: "not-file-stat",
        0x55: "ok",
    }
    return describe_context_stat(result, COPY_UI_STAT_OFFSET, "copy-ui-stat", labels)


def decode_screen(screen):
    if screen is None:
        return ""
    chars = []
    for index, byte in enumerate(screen[:1000]):
        if index and index % 40 == 0:
            chars.append("\n")
        if 1 <= byte <= 26:
            chars.append(chr(ord("A") + byte - 1))
        elif byte == 0:
            chars.append("@")
        elif 32 <= byte <= 95:
            chars.append(chr(byte))
        else:
            chars.append(".")
    return "".join(chars)


def validate_screen(screen, expected_version, expect):
    if screen is None:
        return "missing screen_0400.bin"
    text = decode_screen(screen)
    if "PROBE DONE" not in text:
        return "screen does not show PROBE DONE"
    if expect == "no-uci":
        if "UCI NOT FOUND" not in text:
            return "screen does not show UCI NOT FOUND"
    else:
        if "F1:55 F2:55 F3:55" not in text:
            return "screen does not show success summary F1:55 F2:55 F3:55"
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Validate UCI DMA probe automation artifacts."
    )
    parser.add_argument("run_dir", type=pathlib.Path)
    parser.add_argument(
        "--expect",
        choices=("auto", "no-uci", "success"),
        default="auto",
        help="Expected result mode. auto treats UCI flag $00 as no-uci, $01 as success.",
    )
    parser.add_argument(
        "--version",
        default=f"{EXPECTED_VERSION:02x}",
        help="Expected probe version byte, hex-style text or 0x-prefixed hex.",
    )
    parser.add_argument(
        "--expect-plain",
        choices=("any", "success", "fail"),
        default="any",
        help="Expected result for the pre-CD launcher-style plain STAT check.",
    )
    parser.add_argument(
        "--expect-copy-ui",
        choices=("any", "success", "fail"),
        default="any",
        help="Expected result for COPY_UI_PATH followed by pre-CD plain STAT.",
    )
    args = parser.parse_args()

    version_text = args.version.strip().lower()
    if version_text.startswith("0x"):
        expected_version = int(version_text, 16)
    else:
        expected_version = int(version_text, 16)
    expected_version &= 0xFF

    run_dir = args.run_dir
    result = read_file(artifact_path(run_dir, "probe_results_3000"))
    ram = read_file(artifact_path(run_dir, "dma_load_buf_4000"))
    reu = read_file(artifact_path(run_dir, "reu_fetch_buf_5000"))
    screen = read_file(artifact_path(run_dir, "screen_0400"))
    snapshots = {
        label: read_file(artifact_path(run_dir, label))
        for _, _, label in FILE_BLOCKS
    }

    error = validate_result_header(result, expected_version)
    if error:
        return fail(error)

    if args.expect == "auto":
        expect = "success" if result[4] == 1 else "no-uci"
    else:
        expect = args.expect

    if expect == "no-uci":
        error = validate_no_uci(result)
    else:
        error = validate_success(result, ram, reu, snapshots)
    if error:
        return fail(error)

    error = validate_screen(screen, expected_version, expect)
    if error:
        return fail(error)

    plain_status = result[PLAIN_STAT_OFFSET] if len(result) > PLAIN_STAT_OFFSET else 0
    if args.expect_plain == "success" and plain_status != 0x55:
        return fail(describe_plain_stat(result))
    if args.expect_plain == "fail" and plain_status == 0x55:
        return fail(describe_plain_stat(result))
    copy_ui_status = (
        result[COPY_UI_STAT_OFFSET] if len(result) > COPY_UI_STAT_OFFSET else 0
    )
    if args.expect_copy_ui == "success" and copy_ui_status != 0x55:
        return fail(describe_copy_ui_stat(result))
    if args.expect_copy_ui == "fail" and copy_ui_status == 0x55:
        return fail(describe_copy_ui_stat(result))

    print(f"OK: {run_dir} ({expect}, version ${expected_version:02X})")
    print(f"OK: {describe_plain_stat(result)}")
    print(f"OK: {describe_copy_ui_stat(result)}")
    if expect == "success":
        print(
            "OK: UCI flag, file statuses, RAM batches, file sizes, RAM buffer, "
            "SoftIEC flag, REU fetch buffer, and per-file REU snapshots validated"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
