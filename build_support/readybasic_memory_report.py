#!/usr/bin/env python3
"""Generate the ReadyBASIC proportional RAM/REU memory report."""

from __future__ import annotations

import argparse
import html
import re
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Segment:
    name: str
    start: int
    end: int
    size: int


@dataclass(frozen=True)
class Block:
    label: str
    start: int
    size: int
    kind: str
    detail: str = ""
    commands: tuple[str, ...] = ()

    @property
    def end(self) -> int:
        return self.start + self.size - 1


def parse_int(text: str) -> int:
    value = text.strip()
    if value.startswith("$"):
        return int(value[1:], 16)
    return int(value, 0)


def parse_map_segments(path: Path) -> dict[str, Segment]:
    text = path.read_text(encoding="utf-8", errors="replace")
    seg_re = re.compile(
        r"^\s*([A-Z0-9_]+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+[0-9A-F]{5}\s*$"
    )
    segments: dict[str, Segment] = {}
    in_segments = False
    for line in text.splitlines():
        if line.strip() == "Segment list:":
            in_segments = True
            continue
        if not in_segments:
            continue
        m = seg_re.match(line)
        if m:
            name = m.group(1)
            segments[name] = Segment(
                name=name,
                start=int(m.group(2), 16),
                end=int(m.group(3), 16),
                size=int(m.group(4), 16),
            )
            continue
        if segments and line.strip() == "":
            break
    required = {
        "ENTRY",
        "RESIDENT",
        "REGSEED",
        "HIDDEN",
        "LOWPACK",
        "SLOTPACK1",
        "SLOTPACK2",
        "SPANPACK",
        "OVL1PACK",
        "OVL2PACK",
        "OVL3PACK",
        "OVL4PACK",
        "OVL5PACK",
        "OVL6PACK",
        "BRIDGE",
    }
    missing = sorted(required - set(segments))
    if missing:
        raise SystemExit(f"{path}: missing ReadyBASIC map segments: {', '.join(missing)}")
    return segments


def parse_map_symbols(path: Path, names: set[str]) -> dict[str, int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    out: dict[str, int] = {}
    for name in names:
        m = re.search(rf"(?<!\S){re.escape(name)}\s+([0-9A-F]{{6}})\s+(?:REA|RLA|RLZ)\b", text)
        if not m:
            raise SystemExit(f"{path}: missing map symbol {name}")
        out[name] = int(m.group(1), 16)
    return out


def parse_asm_constants(path: Path) -> dict[str, int]:
    out: dict[str, int] = {}
    src = path.read_text(encoding="utf-8", errors="replace")
    for line in src.splitlines():
        m = re.match(r"^([A-Z0-9_]+)\s*=\s*(\$[0-9A-Fa-f]+|\d+)\s*(?:;.*)?$", line)
        if m:
            out[m.group(1)] = parse_int(m.group(2))
    for name in ("BASIC_START", "BASIC_LIMIT", "RB_SLOT0_BASE", "RB_SLOT1_BASE", "RB_SLOT2_BASE"):
        if name not in out:
            raise SystemExit(f"{path}: missing required constant {name}")
    return out


def parse_memory_config(path: Path) -> dict[str, tuple[int, int]]:
    src = path.read_text(encoding="utf-8", errors="replace")
    mem_re = re.compile(
        r"^\s*([A-Z0-9_]+):\s+file\s*=\s*(?:%O|\"\"),\s*start\s*=\s*\$([0-9A-Fa-f]+),\s*size\s*=\s*\$([0-9A-Fa-f]+)",
        flags=re.MULTILINE,
    )
    out: dict[str, tuple[int, int]] = {}
    for name, start_s, size_s in mem_re.findall(src):
        out[name] = (int(start_s, 16), int(size_s, 16))
    required = {"ENTRY", "RESIDENT", "SLOT0", "SLOT1", "SLOT2", "CMDPACK", "CMDPACK2", "HIDLOAD", "BRLOAD", "REGSEED"}
    missing = sorted(required - set(out))
    if missing:
        raise SystemExit(f"{path}: missing MEMORY regions: {', '.join(missing)}")
    return out


def parse_reu_constants(path: Path) -> dict[str, int]:
    src = path.read_text(encoding="utf-8", errors="replace")
    out: dict[str, int] = {}
    for name, value in re.findall(r"#define\s+(REU_BANK_[A-Z0-9_]+|REU_[A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+|\d+)\b", src):
        out[name] = int(value, 0)
    for name in ("REU_TOTAL_BANKS",):
        if name not in out:
            raise SystemExit(f"{path}: missing {name}")
    return out


def parse_command_descriptors(path: Path) -> list[tuple[str, str]]:
    src = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"rb_command_descriptors:\n(?P<body>.*?)\n; -{20,}", src, flags=re.S)
    if not m:
        raise SystemExit(f"{path}: rb_command_descriptors block not found")
    commands: list[tuple[str, str]] = []
    for line in m.group("body").splitlines():
        line = line.strip()
        if not line.startswith("CMD_"):
            continue
        name_m = re.search(r'"([^"]+)"', line)
        if not name_m:
            continue
        macro = line.split(None, 1)[0]
        commands.append((macro, name_m.group(1)))
    if not commands:
        raise SystemExit(f"{path}: no command descriptor macro lines found")
    return commands


def fmt_hex(value: int, width: int = 4) -> str:
    return f"${value:0{width}X}"


def fmt_range(start: int, end: int, width: int = 4) -> str:
    return f"{fmt_hex(start, width)}-{fmt_hex(end, width)}"


def fmt_size(size: int) -> str:
    if size < 500:
        return f"{size}B"
    kb = size / 1024.0
    if abs(kb - round(kb)) < 0.05:
        return f"{round(kb):.0f}K"
    return f"{kb:.1f}K"


def pct(size: int, total: int) -> float:
    return (size / total) * 100.0


def h(text: object) -> str:
    return html.escape(str(text), quote=True)


def block_div(block: Block, total: int, *, show_range: bool = True, tiny: bool = False) -> str:
    basis = pct(block.size, total)
    min_height = "26px" if tiny or basis < 1.0 else "0"
    commands = ""
    if block.commands:
        commands = "<div class='commands'>" + "".join(f"<span>{h(cmd)}</span>" for cmd in block.commands) + "</div>"
    detail = f"<small>{h(block.detail)}</small>" if block.detail else ""
    range_text = f"<code>{fmt_range(block.start, block.end)}</code>" if show_range else ""
    return (
        f"<div class='mem-block {h(block.kind)}' style='flex-basis:{basis:.4f}%; min-height:{min_height}' "
        f"title='{h(block.label)} {fmt_range(block.start, block.end)} {fmt_size(block.size)}'>"
        f"<div class='block-top'><strong>{h(block.label)}</strong><span>{range_text} {h(fmt_size(block.size))}</span></div>"
        f"{detail}{commands}</div>"
    )


def stacked_map(blocks: list[Block], total: int, *, cls: str = "", show_range: bool = True) -> str:
    return (
        f"<div class='mem-stack {h(cls)}'>"
        + "".join(block_div(block, total, show_range=show_range, tiny=block.size < 500) for block in blocks)
        + "</div>"
    )


def table_rows(blocks: list[Block], *, width: int = 4) -> str:
    rows = []
    for block in blocks:
        commands = ", ".join(block.commands) if block.commands else ""
        rows.append(
            "<tr>"
            f"<td><code>{fmt_range(block.start, block.end, width)}</code></td>"
            f"<td>{h(block.label)}</td>"
            f"<td>{h(fmt_size(block.size))}</td>"
            f"<td>{block.size}B</td>"
            f"<td>{h(block.detail)}</td>"
            f"<td>{h(commands)}</td>"
            "</tr>"
        )
    return "\n".join(rows)


def command_groups(commands: list[tuple[str, str]]) -> dict[str, tuple[str, ...]]:
    groups = {
        "slot0": [],
        "slot1": [],
        "slot2": [],
        "span": [],
        "overlay": [],
        "gfxcore": [],
        "gfxprim": [],
        "gfxspr": [],
        "inputev": [],
        "gfxpoly": [],
        "gfxdl": [],
        "gfxtile": [],
        "sidcore": [],
        "end": [],
    }
    for macro, name in commands:
        if name == "SCRPUT":
            groups["end"].append(name)
        elif macro in ("CMD_LOW", "CMD_LOW_ALL", "CMD_HIDDEN"):
            groups["slot0"].append(name)
        elif macro == "CMD_SLOT1":
            groups["slot1"].append(name)
        elif macro == "CMD_SLOT2":
            groups["slot2"].append(name)
        elif macro == "CMD_SPAN":
            groups["span"].append(name)
        elif macro in ("CMD_OVL1", "CMD_OVL2"):
            groups["overlay"].append(name)
        elif macro == "CMD_GFXCORE":
            groups["gfxcore"].append(name)
        elif macro == "CMD_GFXPRIM":
            groups["gfxprim"].append(name)
        elif macro == "CMD_GFXSPR":
            groups["gfxspr"].append(name)
        elif macro == "CMD_INPUTEV":
            groups["inputev"].append(name)
        elif macro == "CMD_GFXPOLY":
            groups["gfxpoly"].append(name)
        elif macro == "CMD_GFXDL":
            groups["gfxdl"].append(name)
        elif macro == "CMD_GFXTILE":
            groups["gfxtile"].append(name)
        elif macro == "CMD_SIDCORE":
            groups["sidcore"].append(name)
    return {k: tuple(v) for k, v in groups.items()}


def disk_module_blocks() -> list[Block]:
    # Mirrored from build_support/build_readybasic_disk_modules.py. The small
    # integer proof payloads are 21B; RBM3 overlay images contain two 30B
    # stateful entrypoints plus one shared byte of overlay-local state and are
    # stored on $100-byte strides to make spacing visible.
    return [
        Block("rbm.sample1 descriptor", 0x1500, 0x20, "desc", "ZDM1 descriptor streamed by ZMODLD.", ("ZDM1",)),
        Block("rbm.sample2 descriptors", 0x1600, 0x60, "desc", "Three descriptors; submodule 5 appears twice as overlays 1 and 2.", ("ZDM2S", "ZDOV1", "ZDOV2")),
        Block("rbm.sample3 descriptors", 0x1700, 0x3C0, "desc", "Thirty descriptors for the large SEQ package proof.", ("ZSAA-ZUEB",)),
        Block("rbm.sample1 payload", 0x3000, 21, "module-a", "Module 3, submodule 1, slot 1.", ("ZDM1",)),
        Block("rbm.sample2 span payload", 0x3200, 21, "module-b", "Module 4, submodule 2, slots 1+2.", ("ZDM2S",)),
        Block("rbm.sample2 overlay 1", 0x3300, 21, "overlay", "Module 4, submodule 5, overlay 1, slot 2.", ("ZDOV1",)),
        Block("rbm.sample2 overlay 2", 0x3400, 21, "overlay", "Module 4, submodule 5, overlay 2, slot 2.", ("ZDOV2",)),
        Block("rbm.sample3 group A", 0x3800, 0x43D, "module-a", "ZSAA-ZSEB payload records, submodule 6 overlays 1-5 with A/B entrypoints sharing one state byte inside each loaded overlay image.", ("ZSAA-ZSEB",)),
        Block("rbm.sample3 group B", 0x3D00, 0x43D, "module-b", "ZTAA-ZTEB payload records, submodule 7 overlays 1-5 with A/B entrypoints sharing one state byte inside each loaded overlay image.", ("ZTAA-ZTEB",)),
        Block("rbm.sample3 group C", 0x4200, 0x43D, "span", "ZUAA-ZUEB payload records, submodule 8 overlays 1-5 with A/B entrypoints sharing one state byte inside each loaded overlay image and using the slot 1+2 span mask.", ("ZUAA-ZUEB",)),
    ]


def render(ctx: dict[str, object]) -> str:
    seg: dict[str, Segment] = ctx["segments"]  # type: ignore[assignment]
    sym: dict[str, int] = ctx["symbols"]  # type: ignore[assignment]
    cfg: dict[str, tuple[int, int]] = ctx["cfg"]  # type: ignore[assignment]
    const: dict[str, int] = ctx["const"]  # type: ignore[assignment]
    reu: dict[str, int] = ctx["reu"]  # type: ignore[assignment]
    groups: dict[str, tuple[str, ...]] = ctx["groups"]  # type: ignore[assignment]

    basic_start = const["BASIC_START"]
    basic_limit = const["BASIC_LIMIT"]
    basic_free = basic_limit - (basic_start + 2)
    app_window = 0xB600
    slot_size = 0x0800
    cmdpack_start, cmdpack_size = cfg["CMDPACK"]
    cmdpack2_start, cmdpack2_size = cfg["CMDPACK2"]
    hidload_start, hidload_size = cfg["HIDLOAD"]
    brload_start, brload_size = cfg["BRLOAD"]
    regseed_start, regseed_size = cfg["REGSEED"]
    base_builtin_size = (sym["__SPANPACK_LOAD__"] - sym["__LOWPACK_LOAD__"]) + seg["SPANPACK"].size
    built_in_payload_size = (
        base_builtin_size
        + seg["OVL1PACK"].size
        + seg["OVL2PACK"].size
        + seg["OVL3PACK"].size
        + seg["OVL4PACK"].size
        + seg["OVL5PACK"].size
        + seg["OVL6PACK"].size
    )
    gfxspr_off = const["RB_CODE_GFXSPR_OFF"]
    inputev_off = const["RB_CODE_INPUTEV_OFF"]
    gfxpoly_off = const["RB_CODE_GFXPOLY_OFF"]
    gfxdl_off = const["RB_CODE_GFXDL_OFF"]
    gfxtile_off = const["RB_CODE_GFXTILE_OFF"]
    sidcore_off = const["RB_CODE_SIDCORE_OFF"]

    ram_blocks = [
        Block("Zero page", 0x0000, 0x0100, "system", "C64/BASIC/KERNAL zero page; cc65 ZP is not stomped."),
        Block("Stack", 0x0100, 0x0100, "system", "Hardware stack."),
        Block("Vectors, buffers, screen", 0x0200, 0x0E00, "system", "Includes page-3 hooks, input buffer, screen RAM, and low BASIC/system RAM."),
        Block("ReadyBASIC entry", seg["ENTRY"].start, seg["ENTRY"].size, "entry", "Cold/warm discriminator and early copies."),
        Block("ReadyBASIC resident", seg["RESIDENT"].start, seg["RESIDENT"].size, "resident", "Parser, hooks, ROM calls, REU DMA, PROC/FUNC, flow control, command dispatch."),
        Block("BASIC sentinel", basic_start - 1, 1, "sentinel", "Must be zero before stored-program RUN."),
        Block("BASIC workspace", basic_start, basic_limit - basic_start, "basic", "Program text, variables, arrays, strings, and reclaimed cold-load seed space."),
        Block("Under BASIC ROM", 0xA000, 0x2000, "underrom", "Common helper plus three 2K command submodule slots."),
        Block(
            "Bridge and frames",
            seg["BRIDGE"].start,
            0x0600,
            "bridge",
            f"Bridge through ${seg['BRIDGE'].end:04X}, shared frames and buffers through $C5FF.",
        ),
        Block("ReadyOS REU metadata", 0xC600, 0x0200, "readyos", "Allocation table and system metadata; not ReadyBASIC scratch."),
        Block("ReadyOS shim ABI", 0xC800, 0x0200, "shim", "Resident jump table/data."),
        Block("High RAM gap", 0xCA00, 0x0600, "reserved", "Outside app snapshot and below I/O."),
        Block("I/O / color", 0xD000, 0x1000, "io", "REU registers at $DF00-$DF0A when I/O visible."),
        Block("KERNAL ROM", 0xE000, 0x2000, "rom", "Normally visible after banking is restored."),
    ]

    cold_basic_blocks = [
        Block("Sentinel/pad", basic_start - 1, cmdpack_start - (basic_start - 1), "sentinel", "Cold padding before command seed bytes."),
        Block("CMDPACK seed window", cmdpack_start, cmdpack_size, "seed", "Built-in module payload seed; prestashed into the assigned code bank before BASIC owns this RAM."),
        Block("HIDLOAD helper seed", hidload_start, seg["HIDDEN"].size, "seed2", "Copied to $A000 and REU core-bank $3000 shadow."),
        Block("HIDLOAD reserved tail", hidload_start + seg["HIDDEN"].size, hidload_size - seg["HIDDEN"].size, "free", "Reserved load window tail."),
        Block("BRLOAD bridge seed", brload_start, seg["BRIDGE"].size, "bridge", "Copied to $C000 bridge state."),
        Block("BRLOAD reserved tail", brload_start + seg["BRIDGE"].size, brload_size - seg["BRIDGE"].size, "free", "Reserved load window tail."),
        Block("REGSEED registry", regseed_start, seg["REGSEED"].size, "registry", "Header plus 128 command descriptors; prestashed into the assigned core bank."),
        Block("REGSEED reserved tail", regseed_start + seg["REGSEED"].size, regseed_size - seg["REGSEED"].size, "free", "Reserved load window tail."),
        Block("CMDPACK2 overlay seed window", cmdpack2_start, cmdpack2_size, "seed", "Additional built-in overlay payload seed; prestashed into sparse assigned code-bank offsets."),
        Block("Future BASIC bytes", cmdpack2_start + cmdpack2_size, basic_limit - (cmdpack2_start + cmdpack2_size), "basic", "Also reclaimed by BASIC after cold setup."),
    ]

    cmdpack_blocks = [
        Block("Slot 0 payload seed", sym["__LOWPACK_LOAD__"], seg["LOWPACK"].size, "module-a", f"Module 1 system/default payload; runtime {fmt_range(seg['LOWPACK'].start, seg['LOWPACK'].end)}.", groups["slot0"] + groups["end"]),
        Block("Slot 1 payload seed", sym["__SLOTPACK1_LOAD__"], seg["SLOTPACK1"].size, "module-b", f"Module 2 proof, loader, and GFXCORE payload; runtime {fmt_range(seg['SLOTPACK1'].start, seg['SLOTPACK1'].end)}.", groups["slot1"] + groups["gfxcore"]),
        Block("Slot 2 base seed", sym["__SLOTPACK2_LOAD__"], seg["SLOTPACK2"].size, "module-c", f"Module 2 slot-2 proof plus GFXPRIM payload; runtime {fmt_range(seg['SLOTPACK2'].start, seg['SLOTPACK2'].end)}.", groups["slot2"] + groups["gfxprim"]),
        Block("Span seed", sym["__SPANPACK_LOAD__"], seg["SPANPACK"].size, "span", f"Two-slot proof payload; runtime {fmt_range(seg['SPANPACK'].start, seg['SPANPACK'].end)}.", groups["span"]),
        Block("CMDPACK free seed room", sym["__SPANPACK_LOAD__"] + seg["SPANPACK"].size, (cmdpack_start + cmdpack_size) - (sym["__SPANPACK_LOAD__"] + seg["SPANPACK"].size), "free", "Unused cold-load seed capacity."),
    ]
    cmdpack2_blocks = [
        Block("Overlay 1 seed", sym["__OVL1PACK_LOAD__"], seg["OVL1PACK"].size, "overlay", f"Slot-2 replacement overlay; runtime {fmt_range(seg['OVL1PACK'].start, seg['OVL1PACK'].end)}; REU code offset {fmt_hex(gfxspr_off)}.", ("ZOVL1",)),
        Block("Overlay 2 seed", sym["__OVL2PACK_LOAD__"], seg["OVL2PACK"].size, "overlay", f"Slot-2 replacement overlay; runtime {fmt_range(seg['OVL2PACK'].start, seg['OVL2PACK'].end)}; REU code offset {fmt_hex(inputev_off)}.", ("ZOVL2",)),
        Block("Overlay 3 seed", sym["__OVL3PACK_LOAD__"], seg["OVL3PACK"].size, "overlay", f"Slot-2 replacement overlay; runtime {fmt_range(seg['OVL3PACK'].start, seg['OVL3PACK'].end)}; REU code offset {fmt_hex(gfxpoly_off)}.", groups["gfxpoly"]),
        Block("Overlay 4 seed", sym["__OVL4PACK_LOAD__"], seg["OVL4PACK"].size, "overlay", f"Slot-2 replacement overlay; runtime {fmt_range(seg['OVL4PACK'].start, seg['OVL4PACK'].end)}; REU code offset {fmt_hex(gfxdl_off)}.", groups["gfxdl"]),
        Block("Overlay 5 seed", sym["__OVL5PACK_LOAD__"], seg["OVL5PACK"].size, "overlay", f"Slot-2 replacement overlay; runtime {fmt_range(seg['OVL5PACK'].start, seg['OVL5PACK'].end)}; REU code offset {fmt_hex(gfxtile_off)}.", groups["gfxtile"]),
        Block("Overlay 6 seed", sym["__OVL6PACK_LOAD__"], seg["OVL6PACK"].size, "overlay", f"Slot-2 replacement overlay; runtime {fmt_range(seg['OVL6PACK'].start, seg['OVL6PACK'].end)}; REU code offset {fmt_hex(sidcore_off)}.", groups["sidcore"]),
        Block("CMDPACK2 free seed room", sym["__OVL6PACK_LOAD__"] + seg["OVL6PACK"].size, (cmdpack2_start + cmdpack2_size) - (sym["__OVL6PACK_LOAD__"] + seg["OVL6PACK"].size), "free", "Unused cold-load seed capacity."),
    ]

    post_blocks = [
        Block("Resident ReadyBASIC", seg["RESIDENT"].start, seg["RESIDENT"].size, "resident", "Still live in visible RAM."),
        Block("BASIC workspace", basic_start, basic_limit - basic_start, "basic", "Former seed bytes are user BASIC memory now."),
        Block("Under-ROM command slots", 0xA000, 0x2000, "underrom", "Fetched from the assigned code bank on demand."),
        Block("Bridge/frames", 0xC000, 0x0600, "bridge", "Bridge and shared command frames."),
    ]

    slot0_used = seg["LOWPACK"].size
    slot1_used = seg["SLOTPACK1"].size
    slot_blocks = [
        Block("Slot 0 current", 0xA800, slot0_used, "module-a", "Module 1 system/default payload.", groups["slot0"] + groups["end"]),
        Block("Slot 0 free", 0xA800 + slot0_used, slot_size - slot0_used, "free", f"{fmt_size(slot_size - slot0_used)} free inside slot 0."),
        Block("Slot 1 current", 0xB000, slot1_used, "module-b", "Module 2 proof, ZMODLD, and GFXCORE.", groups["slot1"] + groups["gfxcore"]),
        Block("Slot 1 free", 0xB000 + slot1_used, slot_size - slot1_used, "free", f"{fmt_size(slot_size - slot1_used)} free inside slot 1."),
        Block("Slot 2 GFXPRIM current", 0xB800, seg["SLOTPACK2"].size, "module-c", "Base proof plus GFXPRIM; replacement overlays load over this when called.", groups["slot2"] + groups["gfxprim"]),
        Block("Slot 2 GFXPRIM free", 0xB800 + seg["SLOTPACK2"].size, slot_size - seg["SLOTPACK2"].size, "free", f"{fmt_size(slot_size - seg['SLOTPACK2'].size)} free in the GFXPRIM slot image."),
        Block("Slot 2 GFXSPR overlay", 0xB800, seg["OVL1PACK"].size, "overlay", "Replacement overlay for sprite commands.", groups["overlay"][:1] + groups["gfxspr"]),
        Block("Slot 2 GFXSPR free", 0xB800 + seg["OVL1PACK"].size, slot_size - seg["OVL1PACK"].size, "free", f"{fmt_size(slot_size - seg['OVL1PACK'].size)} free in the GFXSPR overlay image."),
        Block("Slot 2 INPUTEV overlay", 0xB800, seg["OVL2PACK"].size, "overlay", "Replacement overlay for polling input commands.", groups["overlay"][1:] + groups["inputev"]),
        Block("Slot 2 INPUTEV free", 0xB800 + seg["OVL2PACK"].size, slot_size - seg["OVL2PACK"].size, "free", f"{fmt_size(slot_size - seg['OVL2PACK'].size)} free in the INPUTEV overlay image."),
        Block("Slot 2 GFXPOLY overlay", 0xB800, seg["OVL3PACK"].size, "overlay", "Replacement overlay for polygon and point-buffer commands.", groups["gfxpoly"]),
        Block("Slot 2 GFXPOLY free", 0xB800 + seg["OVL3PACK"].size, slot_size - seg["OVL3PACK"].size, "free", f"{fmt_size(slot_size - seg['OVL3PACK'].size)} free in the GFXPOLY overlay image."),
        Block("Slot 2 GFXDL overlay", 0xB800, seg["OVL4PACK"].size, "overlay", "Replacement overlay for retained display-list commands.", groups["gfxdl"]),
        Block("Slot 2 GFXDL free", 0xB800 + seg["OVL4PACK"].size, slot_size - seg["OVL4PACK"].size, "free", f"{fmt_size(slot_size - seg['OVL4PACK'].size)} free in the GFXDL overlay image."),
        Block("Slot 2 GFXTILE overlay", 0xB800, seg["OVL5PACK"].size, "overlay", "Replacement overlay for charset, tileset, tilemap, and multicolor-cell commands.", groups["gfxtile"]),
        Block("Slot 2 GFXTILE free", 0xB800 + seg["OVL5PACK"].size, slot_size - seg["OVL5PACK"].size, "free", f"{fmt_size(slot_size - seg['OVL5PACK'].size)} free in the GFXTILE overlay image."),
        Block("Slot 2 SIDCORE overlay", 0xB800, seg["OVL6PACK"].size, "overlay", "Replacement overlay for immediate SID sound commands.", groups["sidcore"]),
        Block("Slot 2 SIDCORE free", 0xB800 + seg["OVL6PACK"].size, slot_size - seg["OVL6PACK"].size, "free", f"{fmt_size(slot_size - seg['OVL6PACK'].size)} free in the SIDCORE overlay image."),
    ]

    reu44_blocks = [
        Block("Header", 0x0000, 0x0010, "registry", "RBPL header and offsets."),
        Block("Reserved metadata", 0x0010, 0x03F0, "reserved", "Common/system metadata before frames."),
        Block("Call/result frame mirror", 0x0400, 0x0200, "bridge", "Current frame snapshots."),
        Block("Debug ring reserve", 0x0600, 0x0200, "reserved", "Parser/command breadcrumbs reserve."),
        Block("Handle directory", 0x0800, 0x0200, "handle", "128 handle descriptors, 4B each."),
        Block("ZP snapshot", 0x0A00, 0x0100, "system", "ReadyBASIC suspend/resume zero page."),
        Block("Stack snapshot", 0x0B00, 0x0100, "system", "ReadyBASIC suspend/resume stack page."),
        Block("Heap bitmap", 0x0C00, 0x0400, "handle", "192 pages tracked; rest reserved."),
        Block("Command descriptors", 0x1000, 0x1000, "registry", "128 x 32-byte descriptor slots."),
        Block("Reserved common", 0x2000, 0x2000, "reserved", "Future per-slot residency/catalog metadata."),
        Block("Typed handle heap", 0x4000, 0xC000, "heap", "48K data heap for buffers and screen handles."),
    ]

    disk_blocks = disk_module_blocks()
    reu45_blocks = [
        Block("Built-in base payloads", 0x0000, base_builtin_size, "module-a", "LOWPACK, SLOTPACK1, SLOTPACK2, and SPANPACK prestashed from CMDPACK."),
        Block("Built-in base free gap", base_builtin_size, 0x1500 - base_builtin_size, "free", "Free before current disk-module descriptor samples."),
        *disk_blocks[:2],
        Block("Free gap", 0x1660, 0x3000 - 0x1660, "free", "Available packed-code space before sample payloads."),
        *disk_blocks[2:],
        Block("Free gap before built-in overlays", 0x463D, gfxspr_off - 0x463D, "free", "Available packed-code space before fixed built-in overlay offsets."),
        Block("Built-in GFXSPR overlay", gfxspr_off, seg["OVL1PACK"].size, "overlay", "Prestashed from CMDPACK2 and fetched into slot 2 when sprite commands run.", groups["overlay"][:1] + groups["gfxspr"]),
        Block("GFXSPR reserved headroom", gfxspr_off + seg["OVL1PACK"].size, 0x0800 - seg["OVL1PACK"].size, "free", "Reserved so the overlay can grow toward a full 2K slot without moving INPUTEV."),
        Block("Built-in INPUTEV overlay", inputev_off, seg["OVL2PACK"].size, "overlay", "Prestashed from CMDPACK2 and fetched into slot 2 when input commands run.", groups["inputev"]),
        Block("INPUTEV reserved headroom", inputev_off + seg["OVL2PACK"].size, 0x0800 - seg["OVL2PACK"].size, "free", "Reserved so the overlay can grow toward a full 2K slot without moving GFXPOLY."),
        Block("Built-in GFXPOLY overlay", gfxpoly_off, seg["OVL3PACK"].size, "overlay", "Prestashed from CMDPACK2 and fetched into slot 2 when polygon commands run.", groups["gfxpoly"]),
        Block("GFXPOLY reserved headroom", gfxpoly_off + seg["OVL3PACK"].size, 0x0800 - seg["OVL3PACK"].size, "free", "Reserved so the overlay can grow toward a full 2K slot without moving GFXDL."),
        Block("Built-in GFXDL overlay", gfxdl_off, seg["OVL4PACK"].size, "overlay", "Prestashed from CMDPACK2 and fetched into slot 2 when display-list commands run.", groups["gfxdl"]),
        Block("GFXDL reserved headroom", gfxdl_off + seg["OVL4PACK"].size, 0x0800 - seg["OVL4PACK"].size, "free", "Reserved so the overlay can grow toward a full 2K slot without moving GFXTILE."),
        Block("Built-in GFXTILE overlay", gfxtile_off, seg["OVL5PACK"].size, "overlay", "Prestashed from CMDPACK2 and fetched into slot 2 when charset/tile commands run.", groups["gfxtile"]),
        Block("GFXTILE reserved headroom", gfxtile_off + seg["OVL5PACK"].size, 0x0800 - seg["OVL5PACK"].size, "free", "Reserved for charset/tile overlay growth."),
        Block("Built-in SIDCORE overlay", sidcore_off, seg["OVL6PACK"].size, "overlay", "Prestashed from CMDPACK2 and fetched into slot 2 when sound commands run.", groups["sidcore"]),
        Block("SIDCORE reserved headroom", sidcore_off + seg["OVL6PACK"].size, 0x0800 - seg["OVL6PACK"].size, "free", "Reserved for immediate sound command growth."),
        Block("Payload bank free tail", sidcore_off + 0x0800, 0x10000 - (sidcore_off + 0x0800), "free", "Remaining space in the current single ReadyBASIC code bank."),
    ]

    reu_overview_blocks = [
        Block("Launcher/system", 0x000000, 0x010000, "readyos", "Bank 0 logical launcher/system state."),
        Block("App snapshots", 0x020000, 24 * 0x10000, "entry", "Historical low-bank snapshot capacity; current snapshots are launcher-assigned and resolved through logical bank 0."),
        Block("Legacy high-bank gap", 0x430000, 0x60000, "underrom", "No current ReadyOS fixed resource assignment here; ReadyBASIC banks and ReadyShell diagnostics/scratch are no longer fixed in this range."),
        Block("Dynamic resources / free", 0x490000, (reu["REU_TOTAL_BANKS"] - 0x49) * 0x10000, "free", "Remaining 16MB REU space, including launcher-assigned ReadyBASIC core/code resource banks."),
    ]

    example_blocks = [
        Block("Submodule A commands", 0xA800, 500, "module-a", "Example: two commands totaling 500B in a 2K slot.", ("DRAW", "SPRITE")),
        Block("Submodule A free", 0xA800 + 500, slot_size - 500, "free", "1.5K free."),
        Block("Submodule B commands", 0xB000, 500, "module-b", "Example: another small 500B command family.", ("MUSIC", "SOUND")),
        Block("Submodule B free", 0xB000 + 500, slot_size - 500, "free", "1.5K free."),
        Block("Submodule C commands", 0xB800, 1024, "module-c", "Example: larger 1K command family in the third slot.", ("FILEOPEN", "FILEREAD", "FILEWRITE")),
        Block("Submodule C free", 0xBC00, slot_size - 1024, "free", "1K free."),
    ]
    example_reu_blocks = [
        Block("Module header/catalog", 0x3600, 192, "registry", "Example descriptors and module directory."),
        Block("Submodule A payload", 0x36C0, 500, "module-a", "Stored compactly in REU, copied to slot 0 when needed."),
        Block("Submodule B payload", 0x38B4, 500, "module-b", "Stored separately, copied to slot 1 when needed."),
        Block("Submodule C payload", 0x3AA8, 1024, "module-c", "Stored once in REU, copied to slot 2 when needed."),
    ]

    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ReadyBASIC Memory Diagrams</title>
  <style>
    :root {{
      --ink: #20242a;
      --muted: #5d6673;
      --paper: #f7f5ee;
      --panel: #fffdf8;
      --line: #d6d0c2;
      --entry: #2c6f8f;
      --resident: #286f4c;
      --basic: #d9b84f;
      --underrom: #8555a6;
      --bridge: #b45b3f;
      --readyos: #425a8a;
      --shim: #27354f;
      --io: #9c3f5b;
      --rom: #59606a;
      --free: #e7e1d5;
      --module-a: #2f7f73;
      --module-b: #b56b2f;
      --module-c: #506db7;
      --overlay: #a34b75;
      --registry: #6d5a9f;
      --heap: #637d34;
      --handle: #3e7f9f;
      --system: #71808f;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      color: var(--ink);
      background: var(--paper);
      font: 15px/1.55 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }}
    header {{
      background: #20242a;
      color: white;
      padding: 32px 20px 24px;
      border-bottom: 6px solid #d9b84f;
    }}
    .page {{ max-width: 1180px; margin: 0 auto; }}
    h1 {{ margin: 0 0 8px; font-size: clamp(2rem, 4vw, 4.2rem); line-height: 1; letter-spacing: 0; }}
    h2 {{ margin: 0 0 12px; font-size: 1.55rem; }}
    h3 {{ margin: 22px 0 10px; font-size: 1.1rem; }}
    p {{ margin: 0 0 14px; }}
    code {{ font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }}
    nav {{ position: sticky; top: 0; z-index: 5; background: rgba(247,245,238,.96); border-bottom: 1px solid var(--line); }}
    nav .page {{ display: flex; flex-wrap: wrap; gap: 8px; padding: 10px 20px; }}
    nav a {{ color: var(--ink); text-decoration: none; border: 1px solid var(--line); padding: 6px 9px; background: var(--panel); border-radius: 6px; }}
    main {{ padding: 22px 20px 50px; }}
    section {{ background: var(--panel); border: 1px solid var(--line); border-radius: 8px; padding: 20px; margin: 0 0 18px; box-shadow: 0 1px 0 rgba(0,0,0,.04); }}
    .lead {{ max-width: 860px; color: #d9dde4; font-size: 1.05rem; }}
    .metrics {{ display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 10px; margin-top: 18px; }}
    .metric {{ background: rgba(255,255,255,.08); border: 1px solid rgba(255,255,255,.2); padding: 12px; border-radius: 8px; }}
    .metric b {{ display: block; font-size: 1.25rem; color: #ffe08a; }}
    .metric span {{ display: block; color: #edf1f5; font-size: .9rem; }}
    .note {{ padding: 12px; background: #f3efe4; border: 1px solid var(--line); border-left: 5px solid #2c6f8f; border-radius: 6px; color: var(--muted); }}
    .grid-2 {{ display: grid; grid-template-columns: 1fr 1fr; gap: 16px; align-items: start; }}
    .mem-stack {{ display: flex; flex-direction: column; min-height: 700px; border: 1px solid #bfb7a7; background: #f0eadf; overflow: hidden; border-radius: 6px; }}
    .mem-stack.short {{ min-height: 360px; }}
    .mem-stack.bank {{ min-height: 520px; }}
    .mem-stack.reu-overview {{ min-height: 620px; }}
    .mem-block {{ display: flex; flex-direction: column; justify-content: center; gap: 4px; padding: 7px 9px; border-bottom: 1px solid rgba(0,0,0,.16); min-height: 34px; overflow: hidden; }}
    .block-top {{ display: flex; gap: 10px; align-items: baseline; justify-content: space-between; min-width: 0; }}
    .block-top strong, .block-top span {{ overflow-wrap: anywhere; }}
    .mem-block small {{ color: rgba(0,0,0,.72); overflow-wrap: anywhere; }}
    .commands {{ display: flex; flex-wrap: wrap; gap: 4px; }}
    .commands span {{ display: inline-block; padding: 1px 5px; border-radius: 999px; background: rgba(255,255,255,.46); border: 1px solid rgba(0,0,0,.12); font-size: .75rem; }}
    .entry {{ background: color-mix(in srgb, var(--entry), white 18%); color: white; }}
    .resident {{ background: color-mix(in srgb, var(--resident), white 14%); color: white; }}
    .basic {{ background: color-mix(in srgb, var(--basic), white 18%); }}
    .underrom {{ background: color-mix(in srgb, var(--underrom), white 18%); color: white; }}
    .bridge {{ background: color-mix(in srgb, var(--bridge), white 16%); color: white; }}
    .readyos {{ background: color-mix(in srgb, var(--readyos), white 18%); color: white; }}
    .shim {{ background: color-mix(in srgb, var(--shim), white 14%); color: white; }}
    .io {{ background: color-mix(in srgb, var(--io), white 17%); color: white; }}
    .rom {{ background: color-mix(in srgb, var(--rom), white 18%); color: white; }}
    .free {{ background: var(--free); color: #4d4a44; }}
    .seed {{ background: #c4903f; color: white; }}
    .seed2 {{ background: #8f8a3b; color: white; }}
    .sentinel {{ background: #e7cc66; color: #2c2a20; }}
    .module-a {{ background: color-mix(in srgb, var(--module-a), white 16%); color: white; }}
    .module-b {{ background: color-mix(in srgb, var(--module-b), white 16%); color: white; }}
    .module-c {{ background: color-mix(in srgb, var(--module-c), white 17%); color: white; }}
    .span {{ background: #7b6db1; color: white; }}
    .overlay {{ background: color-mix(in srgb, var(--overlay), white 18%); color: white; }}
    .registry {{ background: color-mix(in srgb, var(--registry), white 18%); color: white; }}
    .heap {{ background: color-mix(in srgb, var(--heap), white 18%); color: white; }}
    .handle {{ background: color-mix(in srgb, var(--handle), white 18%); color: white; }}
    .system {{ background: color-mix(in srgb, var(--system), white 20%); color: white; }}
    .reserved {{ background: #c9c3b8; }}
    table {{ width: 100%; border-collapse: collapse; margin-top: 12px; }}
    th, td {{ border: 1px solid var(--line); padding: 7px 8px; text-align: left; vertical-align: top; }}
    th {{ background: #eee7d9; }}
    .legend {{ display: flex; flex-wrap: wrap; gap: 8px; margin: 10px 0 0; }}
    .legend span {{ display: inline-flex; align-items: center; gap: 5px; font-size: .86rem; }}
    .swatch {{ width: 14px; height: 14px; border-radius: 3px; border: 1px solid rgba(0,0,0,.2); }}
    .two-map {{ display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }}
    footer {{ color: var(--muted); padding: 0 20px 36px; }}
    @media (max-width: 820px) {{
      .grid-2, .two-map, .metrics {{ grid-template-columns: 1fr; }}
      .mem-stack {{ min-height: 520px; }}
      .block-top {{ display: block; }}
      section {{ padding: 14px; }}
    }}
  </style>
</head>
<body>
<header>
  <div class="page">
    <h1>ReadyBASIC Memory Diagrams</h1>
    <p class="lead">A proportional special report for the current ReadyOS + ReadyBASIC RAM and REU picture: before BASIC is initialized, after seed bytes are reclaimed, and while command modules/submodules rotate through the 6K under-ROM window.</p>
    <div class="metrics">
      <div class="metric"><b>{fmt_hex(basic_start)}</b><span>BASIC start; {basic_free} formula free bytes.</span></div>
      <div class="metric"><b>3 x 2K</b><span>Command submodule slots at $A800, $B000, and $B800.</span></div>
      <div class="metric"><b>loader-assigned</b><span>Runtime registry bank plus payload bank.</span></div>
      <div class="metric"><b>{fmt_size(built_in_payload_size)}</b><span>Current built-in command payload bytes in the assigned payload bank.</span></div>
    </div>
  </div>
</header>
<nav><div class="page">
  <a href="#whole">Whole RAM</a>
  <a href="#before-after">Before/After BASIC</a>
  <a href="#slots">6K Slots</a>
  <a href="#reu">REU</a>
  <a href="#examples">Examples</a>
  <a href="#tables">Tables</a>
</div></nav>
<main class="page">
  <section id="whole">
    <h2>Whole C64 RAM: ReadyOS Contract And ReadyBASIC Runtime</h2>
    <p>ReadyOS snapshots app RAM from <code>$1000-$C5FF</code>. ReadyBASIC stays inside that contract, while using RAM behind BASIC ROM for command payloads and loader-assigned REU resource banks for durable runtime state.</p>
    {stacked_map(ram_blocks, 0x10000)}
  </section>

  <section id="before-after">
    <h2>ReadyBASIC Before And After BASIC Initialization</h2>
    <div class="two-map">
      <div>
        <h3>Before BASIC Is Initialized: Seed Bytes Are Temporary</h3>
        <p>The PRG load image temporarily occupies ranges that will later be BASIC workspace. Cold entry copies those bytes into their runtime homes and REU.</p>
        {stacked_map(cold_basic_blocks, basic_limit - (basic_start - 1), cls="short")}
      </div>
      <div>
        <h3>After BASIC Is Initialized: Workspace Is Reclaimed</h3>
        <p>BASIC owns <code>{fmt_range(basic_start, basic_limit - 1)}</code>. The command registry and payload bytes are authoritative in REU, not in the old load-image addresses.</p>
        {stacked_map(post_blocks, app_window, cls="short")}
      </div>
    </div>
    <h3>Inside The Cold CMDPACK Seed Window</h3>
    <p>This is the original built-in command/module payload portion that is prestashed into the loader-assigned ReadyBASIC payload bank.</p>
    {stacked_map(cmdpack_blocks, cmdpack_size, cls="short")}
    <h3>Inside The Cold CMDPACK2 Overlay Seed Window</h3>
    <p>This second cold-only seed window carries built-in replacement overlays and is stashed into sparse, fixed REU code offsets before BASIC owns the memory.</p>
    {stacked_map(cmdpack2_blocks, cmdpack2_size, cls="short")}
  </section>

  <section id="slots">
    <h2>The 6K Under-ROM Command Window</h2>
    <p>At runtime, command payloads are fetched from REU into one or more fixed 2K slots behind BASIC ROM. Small proof payloads are visually thickened here so they remain visible; the table below preserves exact byte counts.</p>
    {stacked_map(slot_blocks, 3 * slot_size, cls="short")}
  </section>

  <section id="reu">
    <h2>REU Picture</h2>
    <div class="grid-2">
      <div>
        <h3>16MB REU Ownership Overview</h3>
        {stacked_map(reu_overview_blocks, reu['REU_TOTAL_BANKS'] * 0x10000, cls="reu-overview", show_range=False)}
      </div>
      <div>
        <h3>ReadyBASIC Core Resource Bank: Registry/Runtime</h3>
        {stacked_map(reu44_blocks, 0x10000, cls="bank")}
      </div>
    </div>
    <h3>ReadyBASIC Code Resource Bank: Built-In And Disk-Loaded Payloads</h3>
    {stacked_map(reu45_blocks, 0x10000, cls="bank")}
  </section>

  <section id="examples">
    <h2>Illustrative Future Module Packing</h2>
    <p class="note">This section is intentionally not the current measured branch. It shows how a richer module could use the same three 2K RAM slots while storing compact command/submodule payloads in REU.</p>
    <div class="two-map">
      <div>
        <h3>Example In RAM: Three Submodules Live In 6K</h3>
        {stacked_map(example_blocks, 3 * slot_size, cls="short")}
      </div>
      <div>
        <h3>Example In REU: Compact Stored Payloads</h3>
        {stacked_map(example_reu_blocks, 0x1000, cls="short")}
      </div>
    </div>
  </section>

  <section id="tables">
    <h2>Exact Tables</h2>
    <h3>Measured ReadyBASIC Segments</h3>
    <table><thead><tr><th>Range</th><th>Segment</th><th>Display size</th><th>Exact bytes</th><th>Detail</th><th>Commands</th></tr></thead><tbody>
      {table_rows([
        Block("ENTRY", seg["ENTRY"].start, seg["ENTRY"].size, "entry", "Cold/warm entry."),
        Block("RESIDENT", seg["RESIDENT"].start, seg["RESIDENT"].size, "resident", "Visible ReadyBASIC core."),
        Block("HIDDEN", seg["HIDDEN"].start, seg["HIDDEN"].size, "underrom", "Common under-ROM helper."),
        Block("LOWPACK", seg["LOWPACK"].start, seg["LOWPACK"].size, "module-a", "Slot 0 built-in payload.", groups["slot0"] + groups["end"]),
        Block("SLOTPACK1", seg["SLOTPACK1"].start, seg["SLOTPACK1"].size, "module-b", "Slot 1 proof/loader/GFXCORE payload.", groups["slot1"] + groups["gfxcore"]),
        Block("SLOTPACK2", seg["SLOTPACK2"].start, seg["SLOTPACK2"].size, "module-c", "Slot 2 proof/GFXPRIM payload.", groups["slot2"] + groups["gfxprim"]),
        Block("SPANPACK", seg["SPANPACK"].start, seg["SPANPACK"].size, "span", "Two-slot proof payload.", groups["span"]),
        Block("OVL1PACK", seg["OVL1PACK"].start, seg["OVL1PACK"].size, "overlay", "Slot 2 replacement overlay for GFXSPR.", groups["overlay"][:1] + groups["gfxspr"]),
        Block("OVL2PACK", seg["OVL2PACK"].start, seg["OVL2PACK"].size, "overlay", "Slot 2 replacement overlay for INPUTEV.", groups["overlay"][1:] + groups["inputev"]),
        Block("OVL3PACK", seg["OVL3PACK"].start, seg["OVL3PACK"].size, "overlay", "Slot 2 replacement overlay for GFXPOLY.", groups["gfxpoly"]),
        Block("OVL4PACK", seg["OVL4PACK"].start, seg["OVL4PACK"].size, "overlay", "Slot 2 replacement overlay for GFXDL.", groups["gfxdl"]),
        Block("OVL5PACK", seg["OVL5PACK"].start, seg["OVL5PACK"].size, "overlay", "Slot 2 replacement overlay for GFXTILE.", groups["gfxtile"]),
        Block("OVL6PACK", seg["OVL6PACK"].start, seg["OVL6PACK"].size, "overlay", "Slot 2 replacement overlay for SIDCORE.", groups["sidcore"]),
        Block("BRIDGE", seg["BRIDGE"].start, seg["BRIDGE"].size, "bridge", "Persistent bridge state."),
        Block("REGSEED", seg["REGSEED"].start, seg["REGSEED"].size, "registry", "Load-only registry seed."),
      ])}
    </tbody></table>
    <h3>Disk Module Proof Storage</h3>
    <table><thead><tr><th>Assigned code-bank offset</th><th>Item</th><th>Display size</th><th>Exact bytes</th><th>Detail</th><th>Commands</th></tr></thead><tbody>{table_rows(disk_blocks)}</tbody></table>
  </section>
</main>
<footer><div class="page">Generated from <code>obj/readybasic.map</code>, <code>cfg/ready_app_readybasic.cfg</code>, <code>src/apps/readybasic/readybasic.s</code>, and <code>src/lib/reu_mgr.h</code>.</div></footer>
</body>
</html>
"""


def build_context(args: argparse.Namespace) -> dict[str, object]:
    segments = parse_map_segments(args.map)
    symbols = parse_map_symbols(
        args.map,
        {
            "__LOWPACK_LOAD__",
            "__SLOTPACK1_LOAD__",
            "__SLOTPACK2_LOAD__",
            "__SPANPACK_LOAD__",
            "__OVL1PACK_LOAD__",
            "__OVL2PACK_LOAD__",
            "__OVL3PACK_LOAD__",
            "__OVL4PACK_LOAD__",
            "__OVL5PACK_LOAD__",
            "__OVL6PACK_LOAD__",
        },
    )
    const = parse_asm_constants(args.asm)
    cfg = parse_memory_config(args.cfg)
    reu = parse_reu_constants(args.reu_header)
    commands = parse_command_descriptors(args.asm)
    return {
        "segments": segments,
        "symbols": symbols,
        "const": const,
        "cfg": cfg,
        "reu": reu,
        "groups": command_groups(commands),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--map", type=Path, default=ROOT / "obj" / "readybasic.map")
    parser.add_argument("--cfg", type=Path, default=ROOT / "cfg" / "ready_app_readybasic.cfg")
    parser.add_argument("--asm", type=Path, default=ROOT / "src" / "apps" / "readybasic" / "readybasic.s")
    parser.add_argument("--reu-header", type=Path, default=ROOT / "src" / "lib" / "reu_mgr.h")
    parser.add_argument("--html-out", type=Path, default=ROOT / "docs" / "readybasic_memory_diagrams.html")
    args = parser.parse_args()

    ctx = build_context(args)
    html_text = render(ctx)
    out = args.html_out if args.html_out.is_absolute() else ROOT / args.html_out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(html_text, encoding="utf-8")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
