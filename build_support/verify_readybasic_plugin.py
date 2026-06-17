#!/usr/bin/env python3
"""Static guardrails for the ReadyBASIC REU plugin layout."""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ASM = ROOT / "src/apps/readybasic/readybasic.s"
MAP = ROOT / "obj/readybasic.map"
REU_HDR = ROOT / "src/lib/reu_mgr.h"
PRG = ROOT / "bin/readybasic.prg"
MODULE_DIR = ROOT / "obj/readybasic_modules"


def fail(message: str) -> None:
    raise SystemExit(f"readybasic plugin check failed: {message}")


def require(pattern: str, text: str, description: str) -> None:
    if not re.search(pattern, text, re.MULTILINE):
        fail(description)


def parse_segments(map_text: str) -> dict[str, tuple[int, int, int]]:
    segments: dict[str, tuple[int, int, int]] = {}
    for line in map_text.splitlines():
        match = re.match(r"^([A-Z][A-Z0-9_]*)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+", line)
        if match:
            name, start, end, size = match.groups()
            segments[name] = (int(start, 16), int(end, 16), int(size, 16))
    return segments


def main() -> None:
    asm = ASM.read_text()
    reu_hdr = REU_HDR.read_text()
    if not MAP.exists():
        fail("obj/readybasic.map is missing; build bin/readybasic.prg first")
    if not PRG.exists():
        fail("bin/readybasic.prg is missing; build it first")
    segments = parse_segments(MAP.read_text())

    require(r"^BASIC_START\s*=\s*\$2AC1\b", asm, "BASIC_START must be $2AC1")
    require(r"^BASIC_LIMIT\s*=\s*\$A000\b", asm, "BASIC_LIMIT must be $A000")
    require(r"^rb_resolve_reu_banks_hidden:", asm,
            "ReadyBASIC must resolve loader-assigned REU banks at runtime")
    require(r"^rb_reu_core_bank:\s*\.byte\s+0\b", asm,
            "ReadyBASIC core bank must be runtime state")
    require(r"^rb_reu_code_bank:\s*\.byte\s+0\b", asm,
            "ReadyBASIC code bank must be runtime state")
    require(r"lda\s+rb_reu_core_bank", asm,
            "ReadyBASIC must use resolved core bank for REU DMA")
    require(r"lda\s+rb_reu_code_bank", asm,
            "ReadyBASIC must use resolved code bank for REU DMA")
    if re.search(r"RB_REU_(CORE|CODE)_BANK\s*=", asm):
        fail("ReadyBASIC must not define fixed REU core/code banks")
    require(r"^RB_REU_DESC_OFF\s*=\s*\$1000\b", asm, "descriptor table must live at REU offset $1000")
    require(r"^RB_REU_HANDLE_OFF\s*=\s*\$0800\b", asm, "handle directory must live at REU offset $0800")
    require(r"^RB_REU_HEAP_OFF\s*=\s*\$0C00\b", asm, "heap bitmap must live at REU offset $0C00")
    require(r"^RB_REU_COMMON_LIMIT\s*=\s*\$4000\b", asm, "ReadyBASIC common area must reserve 16KB")
    require(r"^RB_REU_DATA_OFF\s*=\s*\$4000\b", asm, "typed heap must start at REU offset $4000")
    require(r"^RB_CMD_DESC_COUNT\s*=\s*128\b", asm, "command registry must expose 128 descriptor slots")
    require(r"^RB_HANDLE_COUNT\s*=\s*128\b", asm, "handle directory must expose 128 live handles")
    require(r"^RB_HEAP_PAGES\s*=\s*192\b", asm, "typed heap must expose 192 pages / 48KB")
    require(r"^RB_REU_RUNTIME_ZP_OFF\s*=\s*\$0A00\b", asm, "runtime ZP snapshot must live at REU offset $0A00")
    require(r"^RB_REU_RUNTIME_STACK_OFF\s*=\s*\$0B00\b", asm, "runtime stack snapshot must live at REU offset $0B00")
    require(r"^SIG_UPPER\s*=\s*3\b", asm, "UPPER signature must replace STRUP")
    require(r"^SIG_LOWER\s*=\s*4\b", asm, "LOWER signature must be registered")
    require(r"^SIG_SCRCAP\s*=\s*14\b", asm, "SCRCAP signature must be registered")
    require(r"^SIG_SCRPUT\s*=\s*15\b", asm, "SCRPUT signature must be registered")
    require(r"^SIG_FADD\s*=\s*16\b", asm, "FADD signature must be registered")
    require(r"^SIG_ZPAUSE\s*=\s*SIG_BUFFREE\b", asm, "ZPAUSE must reuse the one-integer parser signature")
    require(r"^RB_MODULE_GFX\s*=\s*3\b", asm, "graphics commands must use module id 3")
    require(r"^RB_SUBMOD_GFXCORE\s*=\s*16\b", asm, "GFXCORE submodule must be registered")
    require(r"^RB_SUBMOD_GFXPRIM\s*=\s*17\b", asm, "GFXPRIM submodule must be registered")
    require(r"^RB_SUBMOD_GFXSPR\s*=\s*18\b", asm, "GFXSPR submodule must be registered")
    require(r"^RB_SUBMOD_INPUTEV\s*=\s*19\b", asm, "INPUTEV submodule must be registered")
    require(r"^RB_SUBMOD_GFXPOLY\s*=\s*20\b", asm, "GFXPOLY submodule must be registered")
    require(r"^SIG_GFXMODE\s*=\s*19\b", asm, "GFXMODE signature must be registered")
    require(r"^SIG_GFXSURF\s*=\s*20\b", asm, "GFXSURF signature must be registered")
    require(r"^SIG_PLOT\s*=\s*21\b", asm, "PLOT signature must be registered")
    require(r"^SIG_LINE\s*=\s*22\b", asm, "LINE signature must be registered")
    require(r"^SIG_SPRSET\s*=\s*23\b", asm, "SPRSET signature must be registered")
    require(r"^SIG_KEYNONE\s*=\s*24\b", asm, "zero-argument input signature must be registered")
    require(r"^SIG_POLY\s*=\s*25\b", asm, "polygon array signature must be registered")
    require(r"^RB_HANDLE_TYPE_GFXSURF\s*=\s*3\b", asm, "graphics surfaces must use typed handle 3")
    require(r"^RB_HANDLE_TYPE_POINTBUF\s*=\s*4\b", asm, "point buffers must use typed handle 4")
    require(r"^RB_GFX_SCREEN\s*=\s*\$CC00\b", asm, "Bank D graphics screen RAM must start at $CC00")
    require(r"^RB_GFX_SPRITES\s*=\s*\$CA00\b", asm, "Bank D sprite data must start at $CA00")
    require(r"^RB_GFX_BITMAP\s*=\s*\$E000\b", asm, "Bank D bitmap RAM must start at $E000")
    require(r"^RB_GFX_COLOR\s*=\s*\$D800\b", asm, "graphics color RAM must be $D800")
    require(r"^RB_GFX_SPR_PTRS\s*=\s*\$CFF8\b", asm, "sprite pointer table must stay in the $CC00 screen page")
    require(r"CMD_GFXCORE\s+CMD_GFXMODE,\s+SIG_GFXMODE,\s+cmd_gfxmode,\s+\"GFXMODE\"", asm,
            "GFXMODE must be a built-in GFXCORE command")
    require(r"CMD_GFXPRIM\s+CMD_PLOT,\s+SIG_PLOT,\s+cmd_plot,\s+\"PLOT\"", asm,
            "PLOT must be a built-in GFXPRIM command")
    require(r"CMD_GFXSPR\s+CMD_SPRSET,\s+SIG_SPRSET,\s+cmd_sprset,\s+\"SPRSET\"", asm,
            "SPRSET must be a built-in GFXSPR overlay command")
    require(r"CMD_INPUTEV\s+CMD_JOY,\s+SIG_ZFAIL,\s+cmd_joy,\s+\"JOY\"", asm,
            "JOY must be a built-in INPUTEV overlay command")
    require(r"CMD_GFXPOLY\s+CMD_POLY,\s+SIG_POLY,\s+cmd_poly,\s+\"POLY\"", asm,
            "POLY must be a built-in GFXPOLY overlay command")
    require(r"CMD_GFXPOLY\s+CMD_POLYH,\s+SIG_PLOT,\s+cmd_poly,\s+\"POLYH\"", asm,
            "POLYH must be a built-in GFXPOLY overlay command")
    if re.search(r"^\s*RB_GFX_[A-Za-z0-9_]+\s*=\s*\$C[6-9][0-9A-Fa-f]{2}\b", asm, re.MULTILINE):
        fail("graphics-owned Bank D state must not live in the $C600-$C9FF forbidden range")
    require(r"^CMD_ZMODLOAD\s*=\s*28\b", asm, "ZMODLOAD loader command id must stay stable")
    require(r"CMD_SLOT1\s+CMD_ZMODLOAD,\s+SIG_ZHIDDENRAM,\s+cmd_zmodload,\s+\"ZMODLD\"", asm, "ZMODLD loader command must live in module 2 slot 1")
    require(r"^K_OPEN\s*=\s*\$FFC0\b", asm, "ZMODLD must use streamed KERNAL file I/O")
    require(r"^K_CHRIN\s*=\s*\$FFCF\b", asm, "ZMODLD must stream module bytes with CHRIN")
    if re.search(r"\bjsr\s+K_LOAD\b", asm):
        fail("ZMODLD must not PRG-load ReadyBASIC modules")
    require(r"#define\s+REU_RB_CORE\s+14\b", reu_hdr, "REU_RB_CORE type must stay in sync")
    require(r"#define\s+REU_RB_CODE\s+15\b", reu_hdr, "REU_RB_CODE type must stay in sync")
    if "REU_BANK_RB_CORE" in reu_hdr or "REU_BANK_RB_CODE" in reu_hdr:
        fail("ReadyBASIC core/code banks must not be fixed in reu_mgr.h")

    for name in ("ENTRY", "RESIDENT", "LOWPACK", "SLOTPACK1", "SLOTPACK2", "SPANPACK", "OVL1PACK", "OVL2PACK", "OVL3PACK", "HIDDEN", "BRIDGE", "REGSEED"):
        if name not in segments:
            fail(f"map is missing segment {name}")

    resident = segments["RESIDENT"]
    lowpack = segments["LOWPACK"]
    slotpack1 = segments["SLOTPACK1"]
    slotpack2 = segments["SLOTPACK2"]
    spanpack = segments["SPANPACK"]
    ovl1pack = segments["OVL1PACK"]
    ovl2pack = segments["OVL2PACK"]
    ovl3pack = segments["OVL3PACK"]
    hidden = segments["HIDDEN"]
    bridge = segments["BRIDGE"]
    regseed = segments["REGSEED"]

    if resident[0] != 0x1200 or resident[1] >= 0x2AC0:
        fail(f"RESIDENT must fit in $1200-$2ABF, got ${resident[0]:04X}-${resident[1]:04X}")
    if resident[2] > 0x18C0:
        fail(f"RESIDENT grew past command-module slot budget $18C0, got ${resident[2]:04X}")
    if lowpack[0] != 0xA800 or lowpack[1] > 0xAFFF:
        fail(f"command slot 0 must fit under BASIC ROM at $A800-$AFFF, got ${lowpack[0]:04X}-${lowpack[1]:04X}")
    if lowpack[2] > 0x0800:
        fail(f"command slot 0 grew past 2KB budget, got ${lowpack[2]:04X}")
    if slotpack1[0] != 0xB000 or slotpack1[1] > 0xB7FF:
        fail(f"command slot 1 must fit under BASIC ROM at $B000-$B7FF, got ${slotpack1[0]:04X}-${slotpack1[1]:04X}")
    if slotpack1[2] > 0x0800:
        fail(f"command slot 1 grew past 2KB budget, got ${slotpack1[2]:04X}")
    if slotpack2[0] != 0xB800 or slotpack2[1] > 0xBFFF:
        fail(f"command slot 2 must fit under BASIC ROM at $B800-$BFFF, got ${slotpack2[0]:04X}-${slotpack2[1]:04X}")
    if slotpack2[2] > 0x0800:
        fail(f"command slot 2 grew past 2KB budget, got ${slotpack2[2]:04X}")
    if spanpack[0] != 0xB000 or spanpack[1] > 0xBFFF or spanpack[2] > 0x1000:
        fail(f"two-slot payload must fit at $B000-$BFFF, got ${spanpack[0]:04X}-${spanpack[1]:04X} size ${spanpack[2]:04X}")
    if ovl1pack[0] < 0xB800 or ovl1pack[1] > 0xBFFF or ovl1pack[2] > 0x0800:
        fail(f"overlay 1 payload must fit in slot 2, got ${ovl1pack[0]:04X}-${ovl1pack[1]:04X} size ${ovl1pack[2]:04X}")
    if ovl2pack[0] < 0xB800 or ovl2pack[1] > 0xBFFF or ovl2pack[2] > 0x0800:
        fail(f"overlay 2 payload must fit in slot 2, got ${ovl2pack[0]:04X}-${ovl2pack[1]:04X} size ${ovl2pack[2]:04X}")
    if ovl3pack[0] < 0xB800 or ovl3pack[1] > 0xBFFF or ovl3pack[2] > 0x0800:
        fail(f"overlay 3 payload must fit in slot 2, got ${ovl3pack[0]:04X}-${ovl3pack[1]:04X} size ${ovl3pack[2]:04X}")
    if hidden[0] != 0xA000 or hidden[1] > 0xA7FF:
        fail(f"HIDDEN helper/common area must fit in $A000-$A7FF, got ${hidden[0]:04X}-${hidden[1]:04X}")
    if hidden[2] > 0x0800:
        fail(f"HIDDEN helper/common area grew past under-ROM budget $0800, got ${hidden[2]:04X}")
    if bridge[0] != 0xC000 or bridge[1] >= 0xC200:
        fail(f"BRIDGE must stay below relocated shared frames at $C200, got ${bridge[0]:04X}-${bridge[1]:04X}")
    if bridge[2] > 0x01FF:
        fail(f"BRIDGE grew past command-module budget $01FF, got ${bridge[2]:04X}")
    require(r"^RB_CF\s*=\s*\$C200\b", asm, "RB_CF must be relocated to $C200")
    require(r"^RB_RF\s*=\s*\$C300\b", asm, "RB_RF must be relocated to $C300")
    require(r"^RB_DESC_BUF\s*=\s*\$C480\b", asm, "RB_DESC_BUF must be relocated to $C480")
    require(r"^RB_CMDBUF\s*=\s*\$C4A0\b", asm, "RB_CMDBUF must be relocated to $C4A0")
    require(r"^RB_PAGEBUF\s*=\s*\$C500\b", asm, "RB_PAGEBUF must be relocated to $C500")
    require(r"^RB_SLOT0_BASE\s*=\s*\$A800\b", asm, "slot 0 base must be $A800")
    require(r"^RB_SLOT1_BASE\s*=\s*\$B000\b", asm, "slot 1 base must be $B000")
    require(r"^RB_SLOT2_BASE\s*=\s*\$B800\b", asm, "slot 2 base must be $B800")
    require(r"^RB_LOW_BASE\s*=\s*RB_SLOT0_BASE\b", asm, "legacy low base alias must point at slot 0")
    require(r"^RUNTIME_ZP_BUF\s*=\s*\$C400\b", asm, "RUNTIME_ZP_BUF must be $C400")
    require(r"^RUNTIME_STACK_BUF\s*=\s*\$C500\b", asm, "RUNTIME_STACK_BUF must be $C500")
    require(r"^RB_REU_HIDDEN_SHADOW_OFF\s*=\s*\$3000\b",
            asm, "hidden helper shadow must live in ReadyBASIC core REU offset $3000")
    require(r"^RB_CODE_GFXSPR_OFF\s*=\s*\$5000\b",
            asm, "built-in GFXSPR overlay must be stashed at REU code offset $5000")
    require(r"^RB_CODE_INPUTEV_OFF\s*=\s*\$5800\b",
            asm, "built-in INPUTEV overlay must be stashed at REU code offset $5800")
    require(r"^RB_CODE_GFXPOLY_OFF\s*=\s*\$6000\b",
            asm, "built-in GFXPOLY overlay must be stashed at REU code offset $6000")
    if re.search(r"^HIDDEN_SHADOW\s*=", asm, re.MULTILINE):
        fail("ReadyBASIC hidden helper shadow must not consume C64 RAM")
    hidden_shadow_end = 0x3000 + hidden[2] - 1
    if hidden_shadow_end >= 0x4000:
        fail(f"HIDDEN shadow copy must stay inside REU common area, got $3000-${hidden_shadow_end:04X}")
    if regseed[0] != 0x5000 or regseed[2] > 0x1200:
        fail(f"REGSEED must stay within cold seed $5000-$61FF, got ${regseed[0]:04X} size ${regseed[2]:04X}")
    if ovl1pack[0] != 0xB800:
        fail(f"overlay 1 must run as a replacement slot-2 overlay at $B800, got ${ovl1pack[0]:04X}")
    if ovl2pack[0] != 0xB800:
        fail(f"overlay 2 must run as a replacement slot-2 overlay at $B800, got ${ovl2pack[0]:04X}")
    if ovl3pack[0] != 0xB800:
        fail(f"overlay 3 must run as a replacement slot-2 overlay at $B800, got ${ovl3pack[0]:04X}")
    if 0x5000 + ovl1pack[2] > 0x5800:
        fail(f"GFXSPR built-in overlay overflowed its reserved 2KB REU code range, size ${ovl1pack[2]:04X}")
    if 0x5800 + ovl2pack[2] > 0x6000:
        fail(f"INPUTEV built-in overlay overflowed its reserved 2KB REU code range, size ${ovl2pack[2]:04X}")
    if 0x6000 + ovl3pack[2] > 0x6800:
        fail(f"GFXPOLY built-in overlay overflowed its reserved 2KB REU code range, size ${ovl3pack[2]:04X}")
    expected_payload = 0x8000 - 0x1000
    actual_payload = PRG.stat().st_size - 2
    if actual_payload < expected_payload:
        fail(
            "PRG payload does not cover the load-image seed span "
            f"$1000-$7FFF: got ${actual_payload:04X}, need ${expected_payload:04X}"
        )

    for module_name in ("rbm.sample1.seq", "rbm.sample2.seq", "rbm.sample3.seq"):
        module_path = MODULE_DIR / module_name
        if not module_path.exists():
            fail(f"{module_name} is missing; build ReadyBASIC module packages first")
        data = module_path.read_bytes()
        if data[:5] != b"RBM!\x01":
            fail(f"{module_name} must start with RBM! version 1")
        if len(data) >= 2 and data[:2] == b"\x00\xc5":
            fail(f"{module_name} must be SEQ module data, not a PRG with a $C500 load address")
    for legacy_name in ("rbm1.prg", "rbm2.prg"):
        if (MODULE_DIR / legacy_name).exists():
            fail(f"legacy PRG module package still exists: {legacy_name}")

    print("readybasic plugin static check OK")


if __name__ == "__main__":
    main()
