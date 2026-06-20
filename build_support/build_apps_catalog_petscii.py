#!/usr/bin/env python3
"""
Build ReadyOS apps.cfg as a strict lowercase-PETASCII SEQ payload.

Source format is sectioned:

  [system]
  variant_name=precog
  variant_boot_name=
  reu_bank_skip=0

  [launcher]
  load_all_to_reu=0
  runappfirst=
  c64u_image_path=

  [apps]
  9:editor:editor:1
  text editor with clipboard
  ...

Rules:
- Alphabetic source text must be lowercase.
- `[system]` is emitted first, followed by `[launcher]`, then `[apps]`.
- App catalog entries preserve the existing alternating entry/description format.
- Build-time overrides can replace `load_all_to_reu` and `runappfirst`.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from typing import Dict, List, Optional, Tuple


PRG_RE = re.compile(r"[a-z0-9_.-]+")

SECTION_SYSTEM = "system"
SECTION_LAUNCHER = "launcher"
SECTION_APPS = "apps"
VALID_SECTIONS = {SECTION_SYSTEM, SECTION_LAUNCHER, SECTION_APPS}
REU_BANK_SKIP_MAX = 39


def fail(path: str, line_no: int, msg: str) -> None:
    raise ValueError(f"{path}:{line_no}: {msg}")


def has_upper_ascii(text: str) -> bool:
    for ch in text:
        if "A" <= ch <= "Z":
            return True
    return False


def normalize_prg_token(raw: str, path: str, line_no: int) -> str:
    prg = raw.strip()
    if not prg:
        fail(path, line_no, "empty PRG token")
    if has_upper_ascii(prg):
        fail(path, line_no, f"prg token must be lowercase: {raw!r}")
    if "," in prg:
        fail(path, line_no, f"comma suffix not allowed in PRG token: {raw!r}")
    if prg.endswith(".prg"):
        fail(path, line_no, f".prg extension not allowed: {raw!r}")
    if len(prg) == 0 or len(prg) > 12:
        fail(path, line_no, f"prg token length invalid: {raw!r}")
    if not PRG_RE.fullmatch(prg):
        fail(path, line_no, f"invalid prg characters: {raw!r}")
    return prg


def normalize_hotkey_slot(raw: str, path: str, line_no: int) -> str:
    slot = raw.strip()
    if not slot:
        fail(path, line_no, "empty hotkey slot")
    if not slot.isdigit():
        fail(path, line_no, f"hotkey slot must be numeric: {raw!r}")
    if slot == "0" or int(slot, 10) > 9:
        fail(path, line_no, f"hotkey slot must be 1..9: {raw!r}")
    return slot


RESOURCE_NONE = ""
RESOURCE_READYSHELL_OVL = "rsovl"
RESOURCE_READYBASIC_CORE = "rbcore"
VALID_RESOURCES = {RESOURCE_NONE, RESOURCE_READYSHELL_OVL, RESOURCE_READYBASIC_CORE}
RESOURCE_DEP_SUFFIX = "+"


def normalize_resource_token(raw: str, path: str, line_no: int) -> str:
    resource = raw.strip()
    dep_suffix = ""
    if resource.endswith(RESOURCE_DEP_SUFFIX):
        dep_suffix = RESOURCE_DEP_SUFFIX
        resource = resource[:-1].strip()
    if resource not in VALID_RESOURCES or resource == RESOURCE_NONE and dep_suffix:
        fail(path, line_no, f"unknown resource set: {raw!r}")
    return resource + dep_suffix


def validate_dependency_list(raw: str, path: str, line_no: int) -> None:
    saw_item = False
    for part in raw.split(","):
        item = part.strip()
        if not item:
            continue
        if "@" in item:
            name_raw, placement_raw = [p.strip() for p in item.split("@", 1)]
            normalize_prg_token(name_raw, path, line_no)
            if ":" not in placement_raw:
                fail(path, line_no, f"dependency placement missing offset: {item!r}")
            bank_raw, off_raw = [p.strip() for p in placement_raw.split(":", 1)]
            if bank_raw not in {"0", "1", "2"}:
                fail(path, line_no, f"dependency resource bank must be 0..2: {bank_raw!r}")
            try:
                off = int(off_raw, 16)
            except ValueError:
                fail(path, line_no, f"dependency offset must be hex: {off_raw!r}")
            if off < 0 or off > 0xC800 or off % 0x3800:
                fail(path, line_no, f"dependency offset invalid for overlay slot: {off_raw!r}")
        elif ":" in item:
            drive_raw, name_raw = [p.strip() for p in item.split(":", 1)]
            if not drive_raw.isdigit():
                fail(path, line_no, f"dependency drive must be numeric: {drive_raw!r}")
            drive = int(drive_raw, 10)
            if drive < 8 or drive > 11:
                fail(path, line_no, f"dependency drive must be 8..11: {drive}")
            normalize_prg_token(name_raw, path, line_no)
        else:
            normalize_prg_token(item, path, line_no)
        saw_item = True
    if not saw_item:
        fail(path, line_no, "empty dependency list")


def parse_app_entry(line: str, path: str, line_no: int) -> Tuple[int, str, str, str, str]:
    parts = [p.strip() for p in line.split(":")]
    if len(parts) not in (3, 4, 5):
        fail(path, line_no, f"malformed app entry: {line!r}")

    drive_raw, prg_raw, label = parts[:3]
    slot = ""
    resource = RESOURCE_NONE
    if len(parts) == 4:
        if parts[3].isdigit():
            slot = normalize_hotkey_slot(parts[3], path, line_no)
        else:
            resource = normalize_resource_token(parts[3], path, line_no)
    elif len(parts) == 5:
        if parts[3]:
            slot = normalize_hotkey_slot(parts[3], path, line_no)
        resource = normalize_resource_token(parts[4], path, line_no)

    if not drive_raw.isdigit():
        fail(path, line_no, f"drive token must be numeric: {drive_raw!r}")
    drive = int(drive_raw, 10)
    if drive < 8 or drive > 11:
        fail(path, line_no, f"drive must be 8..11: {drive}")

    prg = normalize_prg_token(prg_raw, path, line_no)

    if not label:
        fail(path, line_no, "display name is empty")
    if len(label) > 31:
        fail(path, line_no, f"display name too long ({len(label)} > 31)")

    return drive, prg, label, slot, resource


def validate_lower_text(text: str, path: str, line_no: int, label: str) -> None:
    if has_upper_ascii(text):
        fail(path, line_no, f"{label} must be lowercase: {text!r}")


def parse_key_value(line: str, path: str, line_no: int) -> Tuple[str, str]:
    if "=" not in line:
        fail(path, line_no, f"expected key=value line: {line!r}")
    key, value = [part.strip() for part in line.split("=", 1)]
    if not key:
        fail(path, line_no, f"empty key in line: {line!r}")
    validate_lower_text(key, path, line_no, "key")
    return key, value


def normalize_reu_bank_skip(raw: str, path: str, line_no: int) -> str:
    value = raw.strip()
    if not value.isdigit():
        fail(path, line_no, f"reu_bank_skip must be numeric: {raw!r}")
    skip = int(value, 10)
    if skip < 0 or skip > REU_BANK_SKIP_MAX:
        fail(path, line_no, f"reu_bank_skip must be 0..{REU_BANK_SKIP_MAX}: {skip}")
    return str(skip)


def parse_source(path: str) -> Tuple[Dict[str, str], Dict[str, str], List[Tuple[str, str, str]]]:
    with open(path, "r", encoding="utf-8", errors="strict") as f:
        raw_lines = f.read().splitlines()

    system_cfg: Dict[str, str] = {
        "variant_name": "readyos",
        "variant_boot_name": "",
        "reu_bank_skip": "0",
    }
    launcher_cfg: Dict[str, str] = {
        "load_all_to_reu": "0",
        "runappfirst": "",
        "c64u_image_path": "",
    }
    apps: List[Tuple[str, str, str]] = []

    section: Optional[str] = None
    pending_entry: Optional[str] = None
    pending_entry_line = 0
    pending_desc: Optional[str] = None
    pending_dep_required = False

    for idx, raw in enumerate(raw_lines, start=1):
        line = raw.strip()
        if not line or line.startswith("#") or line.startswith(";"):
            continue
        validate_lower_text(line, path, idx, "source text")

        if line.startswith("[") and line.endswith("]"):
            if pending_desc is not None:
                fail(path, pending_entry_line, "missing dependency list before next section")
            if pending_entry is not None:
                fail(path, pending_entry_line, "missing description line before next section")
            section = line[1:-1].strip()
            if section not in VALID_SECTIONS:
                fail(path, idx, f"unknown section: {line!r}")
            continue

        if section is None:
            fail(path, idx, "content before first section")

        if section == SECTION_SYSTEM:
            key, value = parse_key_value(line, path, idx)
            if key == "reu_bank_skip":
                system_cfg[key] = normalize_reu_bank_skip(value, path, idx)
            elif key in system_cfg:
                system_cfg[key] = value
            continue

        if section == SECTION_LAUNCHER:
            key, value = parse_key_value(line, path, idx)
            if key == "load_all_to_reu":
                if value not in ("0", "1"):
                    fail(path, idx, "load_all_to_reu must be 0 or 1")
                launcher_cfg[key] = value
            elif key == "runappfirst":
                if value:
                    launcher_cfg[key] = normalize_prg_token(value, path, idx)
                else:
                    launcher_cfg[key] = ""
            elif key == "c64u_image_path":
                launcher_cfg[key] = value.strip()
            continue

        if section == SECTION_APPS:
            if pending_desc is not None:
                validate_dependency_list(line, path, idx)
                apps.append((pending_entry or "", pending_desc, line))
                pending_entry = None
                pending_desc = None
                pending_dep_required = False
            elif pending_entry is None:
                drive, prg, label, slot, resource = parse_app_entry(line, path, idx)
                entry = f"{drive}:{prg}:{label}"
                if resource:
                    entry += f":{slot}:{resource}"
                elif slot:
                    entry += f":{slot}"
                pending_entry = entry
                pending_entry_line = idx
                pending_dep_required = resource.endswith(RESOURCE_DEP_SUFFIX)
            else:
                if len(line) > 38:
                    fail(path, idx, f"description too long ({len(line)} > 38)")
                if pending_dep_required:
                    pending_desc = line
                else:
                    apps.append((pending_entry, line, ""))
                    pending_entry = None
                    pending_entry_line = 0

    if pending_desc is not None:
        fail(path, pending_entry_line, "missing dependency list")
    if pending_entry is not None:
        fail(path, pending_entry_line, "missing description line")
    if not apps:
        raise ValueError(f"{path}: no app entries found")
    if len(apps) > 64:
        raise ValueError(f"{path}: too many entries ({len(apps)} > 64)")
    return system_cfg, launcher_cfg, apps


def render_lines(system_cfg: Dict[str, str],
                 launcher_cfg: Dict[str, str],
                 apps: List[Tuple[str, str, str]]) -> List[str]:
    lines = [
        "[system]",
        f"variant_name={system_cfg['variant_name']}",
        f"variant_boot_name={system_cfg['variant_boot_name']}",
        f"reu_bank_skip={system_cfg['reu_bank_skip']}",
        "[launcher]",
        f"load_all_to_reu={launcher_cfg['load_all_to_reu']}",
        f"runappfirst={launcher_cfg['runappfirst']}",
        f"c64u_image_path={launcher_cfg['c64u_image_path']}",
        "[apps]",
    ]
    for entry, desc, deps in apps:
        lines.append(entry)
        lines.append(desc)
        if deps:
            lines.append(deps)
    return lines


def resolve_boot_variant(system_cfg: Dict[str, str]) -> str:
    variant = system_cfg.get("variant_boot_name", "")
    if variant:
        return variant
    return system_cfg.get("variant_name", "")


def asm_escape(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def write_variant_asm(path: str, variant_text: str) -> None:
    out_dir = os.path.dirname(path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(path, "w", encoding="utf-8", errors="strict") as f:
        f.write("; Auto-generated by build_apps_catalog_petscii.py. Do not edit by hand.\n\n")
        f.write("msg_variant:\n")
        f.write(f'    .byte "{asm_escape(variant_text)}"\n')
        f.write("msg_variant_end:\n")


def write_reu_config_asm(path: str, system_cfg: Dict[str, str]) -> None:
    skip = normalize_reu_bank_skip(system_cfg.get("reu_bank_skip", "0"),
                                   "<system_cfg>", 0)
    out_dir = os.path.dirname(path)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(path, "w", encoding="utf-8", errors="strict") as f:
        f.write("; Auto-generated by build_apps_catalog_petscii.py. Do not edit by hand.\n")
        f.write("; This is compiled into the boot/shim image; runtime apps.cfg is not read for this value.\n\n")
        f.write(f"READYOS_REU_BANK_SKIP = {skip}\n")


def petscii_lower_byte(ch: str, path: str, line_no: int) -> int:
    code = ord(ch)
    if ch == "\r" or ch == "\n":
        return 13
    if "a" <= ch <= "z":
        return ord(ch.upper())
    if "A" <= ch <= "Z":
        fail(path, line_no, f"unexpected uppercase letter in encoding pass: {ch!r}")
    if 32 <= code <= 126:
        return code
    fail(path, line_no, f"unsupported character U+{code:04X} ({ch!r})")
    return 0


def encode_petscii_lower(lines: List[str], path: str) -> bytes:
    out = bytearray()
    for i, line in enumerate(lines, start=1):
        for ch in line:
            out.append(petscii_lower_byte(ch, path, i))
        out.append(13)
    return bytes(out)


def main() -> int:
    ap = argparse.ArgumentParser(description="Build ReadyOS apps.cfg PETASCII payload")
    ap.add_argument("--input", required=True, help="Sectioned config text source")
    ap.add_argument("--output", required=True, help="Output binary payload")
    ap.add_argument("--variant-asm-output",
                    help="Optional output asm include for boot variant text")
    ap.add_argument("--reu-config-asm-output",
                    help="Optional output asm include for compiled REU bank skip")
    ap.add_argument("--override-load-all", choices=("0", "1"),
                    help="Override launcher load_all_to_reu")
    ap.add_argument("--override-run-first",
                    help="Override launcher runappfirst prg token")
    args = ap.parse_args()

    try:
        system_cfg, launcher_cfg, apps = parse_source(args.input)
        if args.override_load_all is not None:
            launcher_cfg["load_all_to_reu"] = args.override_load_all
        if args.override_run_first is not None:
            launcher_cfg["runappfirst"] = normalize_prg_token(args.override_run_first,
                                                               "<override>", 0)
        lines = render_lines(system_cfg, launcher_cfg, apps)
        variant_text = resolve_boot_variant(system_cfg)
        payload = encode_petscii_lower(lines, args.input)
    except ValueError as ex:
        print(f"error: {ex}", file=sys.stderr)
        return 1

    out_dir = os.path.dirname(args.output)
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(payload)

    if args.variant_asm_output:
        write_variant_asm(args.variant_asm_output, variant_text)
    if args.reu_config_asm_output:
        write_reu_config_asm(args.reu_config_asm_output, system_cfg)

    print(f"wrote {args.output} ({len(payload)} bytes, {len(apps)} entries)")
    if args.variant_asm_output:
        print(f"wrote {args.variant_asm_output} ({len(variant_text)} chars)")
    if args.reu_config_asm_output:
        print(f"wrote {args.reu_config_asm_output} (reu_bank_skip={system_cfg['reu_bank_skip']})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
