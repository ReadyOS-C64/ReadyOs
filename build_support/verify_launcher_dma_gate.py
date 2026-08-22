#!/usr/bin/env python3
"""Verify the experimental launcher UCI DMA path stays gated off by default."""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8", errors="replace")


def ok(message: str) -> None:
    print(f"[OK] {message}")


def fail(message: str) -> int:
    print(f"[FAIL] {message}")
    return 1


def note(message: str) -> None:
    print(f"[NOTE] {message}")


def require(condition: bool, message: str, failures: list[str]) -> None:
    if condition:
        ok(message)
    else:
        print(f"[FAIL] {message}")
        failures.append(message)


def make_var(makefile: str, name: str) -> str | None:
    match = re.search(rf"^{re.escape(name)}\s*\?=\s*(.+)$", makefile, re.M)
    if match:
        return match.group(1).strip()
    match = re.search(rf"^{re.escape(name)}\s*=\s*(.+)$", makefile, re.M)
    if match:
        return match.group(1).strip()
    return None


def main() -> int:
    failures: list[str] = []
    makefile = read("Makefile")
    launcher = read("src/apps/launcher/launcher.c")
    dma_asm = read("src/apps/launcher/launcher_uci_dma.s")
    lessons = read("ULTIMATEDOS_DMA_LOADING_LESSONS_LEARNT.md")
    setup_states_runner = read(
        "build_support/run_launcher_setup_states_c64u.sh")
    setup_states_plan = read(
        "build_support/launcher_setup_states_ultimate.generated.yaml")

    require(
        "readyos_profiles.py launcher-dma-load --profile $(PROFILE)" in
        (make_var(makefile, "LAUNCHER_DMA_LOAD") or ""),
        "Makefile derives the DMA compile gate from the selected profile",
        failures,
    )
    profile_tool = ROOT / "build_support" / "readyos_profiles.py"
    regular_dma = subprocess.check_output(
        [sys.executable, str(profile_tool), "launcher-dma-load",
         "--profile", "precog-d81"], cwd=ROOT, text=True).strip()
    ultimate_dma = subprocess.check_output(
        [sys.executable, str(profile_tool), "launcher-dma-load",
         "--profile", "precog-ultimate"], cwd=ROOT, text=True).strip()
    require(
        regular_dma == "0" and ultimate_dma == "1",
        "regular D81 compiles portable loading while Ultimate compiles DMA",
        failures,
    )
    require(
        "ifeq ($(LAUNCHER_DMA_LOAD),1)" in makefile
        and "$(APPS_DIR)/launcher/launcher_uci_dma.s" in makefile,
        "launcher_uci_dma.s is linked only by the LAUNCHER_DMA_LOAD make gate",
        failures,
    )
    require(
        "-DLAUNCHER_DMA_LOAD=$(LAUNCHER_DMA_LOAD)" in makefile,
        "launcher compile flags carry the DMA gate into C",
        failures,
    )
    require(
        "#if !READYOS_LAUNCHER_VARIANT_EASYFLASH && LAUNCHER_DMA_LOAD" in launcher,
        "launcher C code keeps DMA logic behind variant and DMA guards",
        failures,
    )
    require(
        'strcmp(key, "dma_loading")' in launcher
        and "launcher_cfg_dma_loading" in launcher,
        "runtime apps.cfg DMA enable remains distinct from the compile gate",
        failures,
    )
    require(
        '"DMA:"' in launcher and "launcher_dma_used" in launcher,
        "experimental DMA UI indicator remains explicit when enabled",
        failures,
    )
    require(
        '"run SETUP app for fast app loading"' in launcher
        and "launcher_dma_advise_setup();" in launcher,
        "configured but unavailable DMA directs users to the SETUP app",
        failures,
    )
    require(
        "launcher_uci_dma_validate_image()" in launcher
        and "_launcher_uci_dma_validate_image" in dma_asm,
        "launcher validates the configured image before advertising DMA:YES",
        failures,
    )
    require(
        "jsr dos_cd_path" in dma_asm
        and dma_asm.count("jsr cd_path_advance\n        ldy #$00") == 2,
        "launcher walks nested host paths without carrying a stale Y index",
        failures,
    )
    require(
        "sty path_segment_len" in dma_asm
        and "lda path_segment_len\n        jsr cd_path_advance" in dma_asm,
        "launcher preserves each component length across asynchronous UCI draining",
        failures,
    )
    require(
        "uci_write_path_cmd" not in dma_asm,
        "launcher preserves exact UltimateDOS host-path bytes",
        failures,
    )
    require(
        "launcher_c64u_image_path[i] = (char)0x5Fu" in launcher,
        "launcher converts target-charset underscores back to host ASCII in the configured path",
        failures,
    )
    require(
        all(token in setup_states_plan for token in (
            "DMA:NO", "DMA:YES", "DMA:ON",
            "RUN SETUP APP FOR FAST APP LOADING",
            "invalid_path_editor_loaded_disk", "DMA LOADING",
        )) and "READYOS_SETUP_TEST" in setup_states_runner,
        "physical launcher matrix covers bad/good paths and IEC/DMA behavior",
        failures,
    )
    require(
        "SUM USB:01 P:42 C:51 F1:55 F2:55 F3:55" in lessons,
        "lessons record the current successful D81 probe summary",
        failures,
    )
    require(
        "A pathless, production-safe Ultimate DOS load" in lessons
        and "not proven" in lessons,
        "lessons preserve the pathless mounted-drive blocker",
        failures,
    )
    require(
        "DOS_CMD_LOAD_REU" in lessons
        and "CTRL_CMD_LOAD_REU" in lessons
        and "CTRL_CMD_GET_DRVINFO" in lessons,
        "lessons distinguish DOS LOAD_REU, Control LOAD_REU, and drive info limits",
        failures,
    )
    require(
        "jsr dos_file_info" in dma_asm
        and "lda data_buf" in dma_asm
        and "sbc #$02" in dma_asm
        and "sta remaining_lo" in dma_asm
        and "sta remaining_hi" in dma_asm,
        "DMA loader derives LOAD_REU length from Ultimate DOS FILE_INFO minus PRG header",
        failures,
    )
    require(
        "lda _launcher_uci_dma_max_len\n        sta remaining_lo" not in dma_asm,
        "DMA loader no longer uses destination slot size as the transfer length",
        failures,
    )
    require(
        "FILE_INFO" in lessons
        and "exactly that payload length" in lessons,
        "lessons document exact-size LOAD_REU requirement for ReadyShell overlays",
        failures,
    )

    map_path = ROOT / "obj" / "launcher.map"
    if map_path.exists():
        map_text = map_path.read_text(encoding="utf-8", errors="replace")
        if "launcher_uci_dma" in map_text:
            note(
                "current launcher.map contains launcher_uci_dma symbols; "
                "the workspace was last built with LAUNCHER_DMA_LOAD=1"
            )
        else:
            ok("current launcher.map has no launcher_uci_dma symbols")
    else:
        ok("obj/launcher.map not present; skipped default map symbol check")

    if failures:
        return fail(f"{len(failures)} launcher DMA gate check(s) failed")
    ok("launcher DMA gate verification passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
