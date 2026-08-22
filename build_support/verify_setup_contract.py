#!/usr/bin/env python3
"""Audit the Ultimate SKU and standalone SETUP packaging contract."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    raise AssertionError(message)


def main() -> int:
    profile_path = ROOT / "cfg/profiles/precog-ultimate.json"
    config_path = ROOT / "cfg/profiles/precog-ultimate.ini"
    profile = json.loads(profile_path.read_text(encoding="utf-8"))
    config = config_path.read_text(encoding="utf-8")
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")
    setup_source = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in sorted((ROOT / "src/setup").glob("*")) if path.is_file()
    )
    matrix_runner = (ROOT / "build_support/run_setup_c64u_matrix.sh").read_text()
    matrix_plan = (ROOT / "build_support/setup_ultimate.generated.yaml").read_text()
    failure_runner = (
        ROOT / "build_support/run_setup_c64u_prereq_failures.sh"
    ).read_text()

    if profile.get("launcher_dma_load") != 1:
        fail("precog-ultimate must compile the DMA launcher")
    if "dma_loading=1" not in config or "c64u_image_path=\n" not in config:
        fail("Ultimate apps.cfg source must enable DMA and begin unconfigured")
    additions = profile["disk_overrides"][0]["append_contents"]
    if additions != [{"artifact": "setup.prg", "name": "setup", "type": "prg",
                     "directory_group": "program"}]:
        fail("Ultimate profile must add only standalone SETUP as an ordinary PRG")
    if "TUI_SETUP = $(TUI_BASE_MENU_MISC)" not in makefile:
        fail("SETUP must link the ReadyOS TUI micromodule set")
    setup_rule = makefile.split("$(SETUP):", 1)[1].split("\n\n", 1)[0]
    if "OVERLAY" in setup_rule or "TUI_NAV" in setup_rule or "ready_app.cfg" in setup_rule:
        fail("SETUP rule must remain standalone and overlay/shim-free")
    if "readyfs_" in setup_source.lower() or "overlay" in setup_source.lower():
        fail("SETUP source must not import ReadyFS architecture or overlays")
    if "rdyset.seq" not in setup_source or "rdyset.bak.seq" not in setup_source:
        fail("SETUP must retain staged config commit/rollback names")
    if "host_char_for_tui" not in setup_source or "exact host bytes" not in setup_source:
        fail("SETUP must normalize host ASCII only in its display copy")
    if "TuiRect header = {0u, 0u, 40u, 4u};" not in setup_source:
        fail("SETUP must use the shallow ReadyOS-style four-row header")
    if "TuiRect frame = {0u, 0u, 40u, 25u};" in setup_source:
        fail("SETUP must not draw a full-screen window border")
    if "tui_clear_line(5u, 0u, 40u, TUI_THEME_BG);" not in setup_source:
        fail("SETUP must leave a blank row below the displayed path")
    if "tui_hline(1u, 5u, 38u" in setup_source:
        fail("SETUP must not draw a separator rule below the displayed path")
    if 'SETUP_C64U_SPEEDS:-1 16 64' not in matrix_runner:
        fail("SETUP physical automation must retain the 1/16/64 MHz matrix")
    for required in ("SETUP_INVALID_PATH_KEYS", "PATH: /USB1", "wait_committed"):
        if required not in matrix_plan:
            fail(f"SETUP physical plan is missing {required}")
    for required in (
        'post_settings "Enabled" "Disabled"',
        'post_settings "Disabled" "Enabled"',
        "trap restore_settings",
        "READYOS_SETUP_TEST",
    ):
        if required not in failure_runner:
            fail(f"SETUP prerequisite automation is missing {required}")
    automation_text = matrix_runner + matrix_plan + failure_runner
    if "vice" in automation_text.lower() and "VICE_TASKS_ROOT" not in automation_text:
        fail("SETUP automation must not contain a VICE acceptance path")

    resolved = json.loads(subprocess.check_output(
        [sys.executable, str(ROOT / "build_support/readyos_profiles.py"),
         "resolve", "--profile", "precog-ultimate", "--version", "0.5"],
        cwd=ROOT, text=True,
    ))
    if resolved["kind"] != "ultimate" or resolved["disks"][0]["image_type"] != "d81":
        fail("precog-ultimate must resolve as one D81 Ultimate SKU")

    map_path = ROOT / "obj/setup.map"
    if map_path.exists():
        map_text = map_path.read_text(encoding="utf-8", errors="replace")
        if "OVERLAY" in map_text:
            fail("SETUP linker map unexpectedly contains overlay segments")
        if "007631" not in map_text and "Segment list:" not in map_text:
            fail("SETUP linker map is malformed")

    print("ULTIMATE SETUP CONTRACT VERIFIED")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"SETUP CONTRACT FAILED: {exc}", file=sys.stderr)
        raise SystemExit(1)
