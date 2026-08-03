#!/usr/bin/env python3
"""
Verify that the shared ReadyOS shim remains 512 bytes, preserves ABI anchor
locations, and that cartridge packaging consumes the configured image.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BOOT_ASM = ROOT / "src" / "boot" / "boot_asm.s"
EASYFLASH_SHIM_SRC = ROOT / "src" / "boot" / "easyflash_shim.s"
SHIM_INC = ROOT / "src" / "boot" / "readyos_shim.inc"
EASYFLASH_SHIM_BIN = ROOT / "bin" / "easyflash_shim.bin"

EXPECTED_SIZE = 512

ABI_CHECKS = (
    ("jt_load_disk", 0x000, bytes.fromhex("4c40c8")),
    ("jt_load_reu", 0x003, bytes.fromhex("4c60c8")),
    ("jt_run_app", 0x006, bytes.fromhex("4c0010")),
    ("jt_preload", 0x009, bytes.fromhex("4c80c8")),
    ("jt_return", 0x00C, bytes.fromhex("4c00c9")),
    ("jt_switch", 0x00F, bytes.fromhex("4c40c9")),
    ("jt_reserved_noop", 0x012, bytes.fromhex("4cffc9")),
    ("jt_fetch_bank", 0x015, bytes.fromhex("4cf0c8")),
    ("jt_deprecated_log", 0x018, bytes.fromhex("4cffc9")),
    ("jt_mark_loaded", 0x01B, bytes.fromhex("4cc0c9")),
    ("data_filename_len", 0x021, bytes([0x08])),
    ("storage_drive_default", 0x039, bytes([0x08])),
    ("stash_uses_logical_setup", 0x0E0, bytes.fromhex("2060c9")),
    ("fetch_uses_logical_setup", 0x0F0, bytes.fromhex("2060c9")),
    ("logical_setup_entry", 0x160, bytes.fromhex("c900d006ad3b")),
    ("raw_setup_entry", 0x1A0, bytes.fromhex("8d06df")),
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


def parse_byte_token(token: str, constants: dict[str, int]) -> bytes:
    if token.startswith('"') and token.endswith('"'):
        return token[1:-1].encode("ascii")
    if token.startswith("$"):
        return bytes([int(token[1:], 16)])
    if token in constants:
        return bytes([constants[token] & 0xFF])
    m = re.fullmatch(r"([A-Za-z_][A-Za-z0-9_]*)\s*\+\s*(\d+)", token)
    if m and m.group(1) in constants:
        return bytes([(constants[m.group(1)] + int(m.group(2))) & 0xFF])
    return bytes([int(token, 0)])


def parse_shim_bytes(text: str, *, start_label: str, constants: dict[str, int] | None = None) -> bytes:
    constants = dict(constants or {})
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
        m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(\$[0-9A-Fa-f]+|\d+)\s*$", stripped)
        if m:
            raw_value = m.group(2)
            constants[m.group(1)] = int(raw_value[1:], 16) if raw_value.startswith("$") else int(raw_value, 10)
            continue
        if stripped.startswith(".include"):
            continue
        if ".byte" not in stripped:
            continue
        payload = stripped.split(".byte", 1)[1]
        for token in split_tokens(payload):
            data.extend(parse_byte_token(token, constants))
    return bytes(data)


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_config_value(path: Path) -> int:
    if not path.exists():
        return 0
    text = path.read_text(encoding="utf-8")
    m = re.search(r"(?m)^READYOS_REU_BANK_SKIP\s*=\s*(\$[0-9A-Fa-f]+|\d+)\s*$", text)
    if not m:
        return 0
    raw_value = m.group(1)
    return int(raw_value[1:], 16) if raw_value.startswith("$") else int(raw_value, 10)


def read_current_shim(*, reu_bank_skip: int = 0) -> bytes:
    return parse_shim_bytes(
        "shim_data:\n" + SHIM_INC.read_text(encoding="utf-8"),
        start_label="shim_data:",
        constants={"READYOS_REU_BANK_SKIP": reu_bank_skip},
    )


def require_include_pattern(path: Path, pattern: str, description: str) -> None:
    text = path.read_text(encoding="utf-8")
    if re.search(pattern, text, re.MULTILINE) is None:
        fail(f"{description} missing in {path.relative_to(ROOT)}")
    ok(f"{description} present in {path.relative_to(ROOT)}")


def verify_exact_image(label: str, data: bytes, *, reu_bank_skip: int = 0) -> None:
    if len(data) != EXPECTED_SIZE:
        fail(f"{label} size changed ({len(data)} != {EXPECTED_SIZE})")
    ok(f"{label} size is {EXPECTED_SIZE} bytes")

    digest = sha256_hex(data)
    ok(f"{label} SHA-256 {digest}")

    for name, offset, expected in ABI_CHECKS:
        actual = data[offset:offset + len(expected)]
        if actual != expected:
            fail(
                f"{label} ABI field {name} changed at ${0xC800 + offset:04X} "
                f"({actual.hex()} != {expected.hex()})"
            )
    readyos_actual = data[0x03B]
    readyos_expected = (reu_bank_skip + 1) & 0xFF
    if readyos_actual != readyos_expected:
        fail(f"{label} ReadyOS bank changed at $C83B ({readyos_actual} != {readyos_expected})")
    if bytes.fromhex("a9b88d08df") not in data[0x1A0:0x1C0]:
        fail(f"{label} snapshot length is not $B800")
    for address_lo in (0x07, 0x08, 0x09, 0x0B, 0x0C):
        if bytes((0x8D, address_lo, 0xC0)) in data:
            fail(f"{label} reintroduced an obsolete preload trace store to $C0{address_lo:02X}")

    padding_ranges = (
        (0x05C, 0x060), (0x069, 0x080), (0x0B6, 0x0E0),
        (0x0E9, 0x0F0), (0x0F9, 0x100), (0x11F, 0x140),
        (0x15B, 0x160), (0x19E, 0x1A0), (0x1BE, 0x1C0),
        (0x1FB, 0x1FF),
    )
    padding_bytes = sum(end - start for start, end in padding_ranges)
    if padding_bytes != 129:
        fail(f"internal shim padding accounting changed ({padding_bytes} != 129)")
    for start, end in padding_ranges:
        if any(data[start:end]):
            fail(
                f"{label} executable padding is no longer clear at "
                f"${0xC800 + start:04X}-${0xC800 + end - 1:04X}"
            )
    ok(f"{label} ABI anchor bytes match expected layout")
    ok(f"{label} has no obsolete preload trace stores in app RAM")
    ok(f"{label} has 129 verified executable-padding bytes")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--check-easyflash-bin",
        action="store_true",
        help="also verify bin/easyflash_shim.bin matches the canonical shim bytes",
    )
    args = ap.parse_args()

    current = read_current_shim(reu_bank_skip=0)

    verify_exact_image("readyos_shim.inc", current, reu_bank_skip=0)

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
        ef_skip = read_config_value(ROOT / "src" / "generated" / "readyos_easyflash_reu_config.inc")
        configured = read_current_shim(reu_bank_skip=ef_skip)
        verify_exact_image("easyflash_shim.bin", built, reu_bank_skip=ef_skip)
        if built != configured:
            fail("easyflash_shim.bin diverged from readyos_shim.inc bytes")
        ok("easyflash_shim.bin is byte-identical to readyos_shim.inc")

    print("PASS: ReadyOS shim size, ABI anchors, and configured bytes are valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
