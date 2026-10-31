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
    expected_setup = {"artifact": "setup.prg", "name": "setup", "type": "prg",
                      "directory_group": "program"}
    if not additions or additions[0] != expected_setup:
        fail("Ultimate profile must add standalone SETUP first as an ordinary PRG")
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
    if (resolved["kind"] != "ultimate" or len(resolved["disks"]) != 2 or
            [d["drive"] for d in resolved["disks"]] != [8, 9] or
            any(d["image_type"] != "d81" for d in resolved["disks"])):
        fail("precog-ultimate must resolve as a D81 pair on drives 8 and 9")
    import readyos_profiles
    disks = readyos_profiles.load_profile("precog-ultimate")["disks"]
    boot_names = {e["name"] for e in disks[0]["contents"]}
    if not {"readybasic", "game2048", "deminer", "dizzy", "cal26", "rbm.sample1", "rbm.media", "setup"} <= boot_names:
        fail("Ultimate apps, games, REL-backed apps and module packages must stay on drive 8")
    demos = [e for e in disks[1]["contents"] if readyos_profiles.disk_directory_group(
        e["name"], e["type"], e.get("directory_group")) == "readybasic_example"]
    extras = {e["name"]: e["type"] for e in disks[1]["contents"] if e not in demos}
    if len(demos) != 44 or extras != {
            "c64os": "prg", "gdos64": "prg", "app.c64os": "seq", "app.gdos64": "seq"}:
        fail("Ultimate drive 9 must contain 44 examples and exactly the optional C64OS/GDOS apps/manifests")
    for name in ("c64os", "gdos64"):
        manifest = ROOT / f"cfg/profiles/precog-ultimate-app-{name}.txt"
        if not manifest.read_text().startswith(f"9:{name}:"):
            fail(f"Ultimate optional {name} manifest must load its PRG from drive 9")
    regular = readyos_profiles.load_profile("precog-d81")
    regular_catalog = readyos_profiles.parse_catalog_entries(regular, None)
    if {"c64os", "gdos64"} & readyos_profiles.packaged_apps(regular, regular_catalog):
        fail("Stock regular D81 must not package the relocated bridge apps")
    for suffix, bridge in (("c64os", "c64os"), ("gdos", "gdos64"), ("geos20", "geos20")):
        dedicated = readyos_profiles.load_profile("precog-ultimate-" + suffix)
        catalog = readyos_profiles.parse_catalog_entries(dedicated, None)
        bridge_names = {"c64os", "gdos64", "geos20"}
        if readyos_profiles.packaged_apps(dedicated, catalog) & bridge_names != {bridge}:
            fail(f"Dedicated Ultimate {suffix} must package only its matching bridge")
        if any(e["name"] in bridge_names or e["name"] in {"app.c64os", "app.gdos64"}
               for e in dedicated["disks"][1]["contents"]):
            fail(f"Dedicated Ultimate {suffix} must not inherit optional drive-9 bridge copies")
    if any(readyos_profiles.disk_directory_group(e["name"], e["type"], e.get("directory_group")) ==
           "readybasic_example" for e in disks[0]["contents"]):
        fail("Ultimate BASIC examples must be on drive 9")

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
