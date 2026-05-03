#!/usr/bin/env python3
"""
Verify that the shared ReadyOS shim remains byte-identical to the historical
non-cartridge baseline and that cartridge packaging consumes that exact image.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BOOT_ASM = ROOT / "src" / "boot" / "boot_asm.s"
EASYFLASH_SHIM_SRC = ROOT / "src" / "boot" / "easyflash_shim.s"
SHIM_INC = ROOT / "src" / "boot" / "readyos_shim.inc"
EASYFLASH_SHIM_BIN = ROOT / "bin" / "easyflash_shim.bin"

HISTORICAL_BASE_COMMIT = "a3dc03a"
EXPECTED_SIZE = 512
EXPECTED_SHA256 = "cabc712eb2ea54bcd5aa1aedfeec3f69b78326092e1adaee465522d30696edbc"

ABI_CHECKS = (
    ("jt_load_disk", 0x000, bytes.fromhex("4c40c8")),
    ("jt_load_reu", 0x003, bytes.fromhex("4c60c8")),
    ("jt_run_app", 0x006, bytes.fromhex("4c0010")),
    ("jt_preload", 0x009, bytes.fromhex("4c80c8")),
    ("jt_return", 0x00C, bytes.fromhex("4c00c9")),
    ("jt_switch", 0x00F, bytes.fromhex("4c40c9")),
    ("jt_stash_cur", 0x012, bytes.fromhex("4cc0c8")),
    ("jt_fetch_bank", 0x015, bytes.fromhex("4cf0c8")),
    ("jt_log_byte", 0x018, bytes.fromhex("4ce0c9")),
    ("data_filename_len", 0x021, bytes([0x08])),
    ("storage_drive_default", 0x039, bytes([0x08])),
)


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


def ok(message: str) -> None:
    print(f"OK: {message}")


def strip_comment(line: str) -> str:
    out: list[str] = []
    in_string = False
    for ch in line:
        if ch == '"':
            in_string = not in_string
            out.append(ch)
        elif ch == ";" and not in_string:
            break
        else:
            out.append(ch)
    return "".join(out)


def split_tokens(payload: str) -> list[str]:
    tokens: list[str] = []
    current: list[str] = []
    in_string = False
    for ch in payload:
        if ch == '"':
            in_string = not in_string
            current.append(ch)
        elif ch == "," and not in_string:
            token = "".join(current).strip()
            if token:
                tokens.append(token)
            current = []
        else:
            current.append(ch)
    token = "".join(current).strip()
    if token:
        tokens.append(token)
    return tokens


def parse_byte_token(token: str) -> bytes:
    if token.startswith('"') and token.endswith('"'):
        return token[1:-1].encode("ascii")
    if token.startswith("$"):
        return bytes([int(token[1:], 16)])
    return bytes([int(token, 0)])


def parse_shim_bytes(text: str, *, start_label: str) -> bytes:
    data = bytearray()
    active = False
    for raw_line in text.splitlines():
        line = raw_line.rstrip("\n")
        if not active:
            if line.strip().startswith(start_label):
                active = True
            continue
        stripped = strip_comment(line).strip()
        if not stripped:
            continue
        if stripped.startswith(".include"):
            continue
        if ".byte" not in stripped:
            continue
        payload = stripped.split(".byte", 1)[1]
        for token in split_tokens(payload):
            data.extend(parse_byte_token(token))
    return bytes(data)


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_historical_shim() -> bytes:
    proc = subprocess.run(
        ["git", "show", f"{HISTORICAL_BASE_COMMIT}:src/boot/boot_asm.s"],
        cwd=str(ROOT),
        check=True,
        text=True,
        capture_output=True,
    )
    return parse_shim_bytes(proc.stdout, start_label="shim_data:")


def read_current_shim() -> bytes:
    return parse_shim_bytes("shim_data:\n" + SHIM_INC.read_text(encoding="utf-8"), start_label="shim_data:")


def require_include_pattern(path: Path, pattern: str, description: str) -> None:
    text = path.read_text(encoding="utf-8")
    if re.search(pattern, text, re.MULTILINE) is None:
        fail(f"{description} missing in {path.relative_to(ROOT)}")
    ok(f"{description} present in {path.relative_to(ROOT)}")


def verify_exact_image(label: str, data: bytes) -> None:
    if len(data) != EXPECTED_SIZE:
        fail(f"{label} size changed ({len(data)} != {EXPECTED_SIZE})")
    ok(f"{label} size is {EXPECTED_SIZE} bytes")

    digest = sha256_hex(data)
    if digest != EXPECTED_SHA256:
        fail(f"{label} SHA-256 changed ({digest} != {EXPECTED_SHA256})")
    ok(f"{label} SHA-256 matches historical baseline")

    for name, offset, expected in ABI_CHECKS:
        actual = data[offset:offset + len(expected)]
        if actual != expected:
            fail(
                f"{label} ABI field {name} changed at ${0xC800 + offset:04X} "
                f"({actual.hex()} != {expected.hex()})"
            )
    ok(f"{label} ABI anchor bytes match historical layout")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--check-easyflash-bin",
        action="store_true",
        help="also verify bin/easyflash_shim.bin matches the canonical shim bytes",
    )
    args = ap.parse_args()

    historical = read_historical_shim()
    current = read_current_shim()

    verify_exact_image("historical shim", historical)
    verify_exact_image("readyos_shim.inc", current)

    if historical != current:
        fail("readyos_shim.inc bytes diverged from the historical non-cartridge shim")
    ok("readyos_shim.inc is byte-identical to the historical shim")

    require_include_pattern(
        BOOT_ASM,
        r"(?m)^shim_data:\s*\n\.include \"readyos_shim\.inc\"$",
        "boot shim include handoff",
    )
    require_include_pattern(
        EASYFLASH_SHIM_SRC,
        r"(?m)^\.include \"readyos_shim\.inc\"$",
        "EasyFlash shim include",
    )

    if args.check_easyflash_bin:
        if not EASYFLASH_SHIM_BIN.exists():
            fail("bin/easyflash_shim.bin is missing; build EasyFlash artifacts first")
        built = EASYFLASH_SHIM_BIN.read_bytes()
        verify_exact_image("easyflash_shim.bin", built)
        if built != current:
            fail("easyflash_shim.bin diverged from readyos_shim.inc bytes")
        ok("easyflash_shim.bin is byte-identical to readyos_shim.inc")

    print("PASS: ReadyOS shim matches the historical baseline and ABI")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
