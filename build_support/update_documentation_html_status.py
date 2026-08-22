#!/usr/bin/env python3
"""Add or refresh visible current/historical status banners in documentation HTML.

The HTML reports include both live generated documentation and intentionally
preserved design/evidence snapshots. Keeping the classification here makes it
possible to audit the complete HTML corpus without deleting or silently
rewriting historical measurements.
"""

import argparse
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
START = "<!-- READYOS-DOC-STATUS:START -->"
END = "<!-- READYOS-DOC-STATUS:END -->"

CURRENT = {
    "docs/DOCUMENTATION_INDEX.html": "Current documentation index, audited against the production-stamped 0.2.5 tree on 2026-08-21.",
    "docs/ReadyOS_SHIM_ARCHITECTURE_0.2.5.html": "Current 0.2.5 schema-v5 shim/ReadyOS-bank architecture, including the resident-versus-REU authority split and live token lookup.",
    "docs/ultimate_dos_dma_loading.html": "Current C64 Ultimate DOS DMA behavior. The launcher path is opt-in and disk fallback remains part of the contract.",
    "docs/uci_tester.html": "Current UCI Tester controls, protocol rules, catalog behavior, selectable examples, and physical-Ultimate workflows.",
    "docs/ReadyShellArchitecture.html": "Current ReadyShell architecture counterpart generated from ReadyShellArchitecture.md.",
    "docs/ReadyShellHostTesting.html": "Current ReadyShell host-testing counterpart generated from ReadyShellHostTesting.md.",
    "docs/readybasic_memory_diagrams.html": "Current generated ReadyBASIC memory visualization. Regenerate after linker or module-layout changes.",
    "docs/readyshell_overlay_inventory.html": "Current artifact-backed ReadyShell overlay inventory for the version/profile named in the report.",
    "docs/reports/easyflash_boot_flow.html": "Current EasyFlash boot-flow counterpart generated from easyflash_boot_flow.md.",
    "privatedocs/reports/readyshell_overlay_inventory.html": "Current private counterpart of the artifact-backed ReadyShell overlay inventory.",
    "src/apps/readybasic/readybasic_current_design.html": "Current ReadyBASIC implementation and custom assembler/linker-shape contract generated from READYBASIC_CURRENT_DESIGN.md.",
    "src/apps/readybasic/readybasic_lifecycle_visual_guide.html": "Current ReadyBASIC lifecycle and REU architecture counterpart generated from READYBASIC_LIFECYCLE_AND_REU_ARCHITECTURE.md.",
    "src/apps/readybasic/readybasic_making_command_guide.html": "Current ReadyBASIC command-authoring counterpart generated from READYBASIC_MAKING_COMMAND_GUIDE.md.",
}

HISTORICAL = {
    "ReadyOSREUPhase1Completed.html": "Preserved completed-phase record. Its full commented shim appendix is byte-verified current; other values describe the phase at the time and are not a live 0.2.5 memory contract.",
    "docs/ReadyOS SHIM Architecture Report (0.2).html": "Preserved 0.2.4 shim report with its full commented source appendix refreshed and byte-verified against the current shim. ReadyOS_SHIM_ARCHITECTURE_0.2.5.html is the current architecture contract.",
    "docs/reports/ReadyOSREUPhase1Completed.html": "Preserved completed-phase record. Its full commented shim appendix is byte-verified current; other values describe the phase at the time and are not a live 0.2.5 memory contract.",
    "docs/reports/function_key_audit.html": "Preserved function-key audit snapshot. Revalidate profile/app coverage before treating counts as current.",
    "docs/reports/readyos_memory_size_comparison_0_1_5_vs_now.html": "Preserved 0.1.5-to-0.2.4 comparison. Sections saying “current” mean the 0.2.4 audit point, not the present 0.2.5 tree.",
    "docs/reports/readyos_shim_architecture_report_v3.html": "Preserved 0.2.4 v3 report with its full commented source appendix refreshed and byte-verified current. Use ReadyOS_SHIM_ARCHITECTURE_0.2.5.html for current architecture.",
    "docs/reports/simplefiles_once_bss_resume_headroom_0_1_8w.html": "Preserved 0.1.8W measurement report; it is evidence from that build, not current headroom.",
    "privatedocs/reports/easyflash_boot_flow.html": "Preserved older styled EasyFlash report. The current public Markdown/HTML boot-flow pair supersedes its fixed-slot and app-count measurements.",
    "privatedocs/reports/function_key_audit.html": "Preserved function-key audit snapshot. Revalidate profile/app coverage before treating counts as current.",
    "privatedocs/reports/future_petscii_control_character_options_for_readyos_and_readyshell.html": "Design exploration retained for reference; it is not a current runtime contract.",
    "privatedocs/reports/non_ignored_file_inventory.html": "Point-in-time repository inventory retained for reference; filenames and generated artifacts may have changed.",
    "privatedocs/reports/old_readybasic_memory_rearrangement_implemented.html": "Historical ReadyBASIC implementation record. READYBASIC_CURRENT_DESIGN.md and the generated memory diagrams are authoritative now.",
    "privatedocs/reports/potential_future_readyos_ram_maximization_plan.html": "Future design exploration retained for reference; no proposed memory move is current unless reflected in the canonical memory map.",
    "privatedocs/reports/readybasic_design_ideas.html": "Pre/current-design exploration retained for provenance. READYBASIC_CURRENT_DESIGN.md supersedes it.",
    "privatedocs/reports/readyos_c64os_interop_design.html": "Historical interop proposal. Fixed-slot and schema-4 control-bank descriptions are superseded by the schema-v5 combined ReadyOS bank.",
    "privatedocs/reports/readyos_c64os_interop_design_v2.html": "Versioned interop proposal retained for reference; it is not a current ReadyOS memory or compatibility contract.",
    "privatedocs/reports/readyos_memory_layout_print.html": "Historical memory-layout options report. Use the canonical private MEMORY_MAP.md for current spatial layout and sizes.",
    "privatedocs/reports/readyos_memory_size_comparison_0_1_5_vs_now.html": "Preserved 0.1.5-to-0.2.4 comparison. “Current” values refer to that audit point, not the present 0.2.5 tree.",
    "privatedocs/reports/readyos_shim_architecture_report.html": "Historical shim report retained for ABI evolution context; its fixed app-slot REU layout is superseded by the current public 0.2.5 shim report.",
    "privatedocs/reports/readyos_shim_architecture_report_v2.html": "Historical shim v2 report retained for ABI evolution context; use docs/ReadyOS_SHIM_ARCHITECTURE_0.2.5.html for current behavior.",
    "privatedocs/reports/readyos_shim_architecture_report_v3 copy.html": "Preserved intermediate v3 copy. It is intentionally not normalized into the current contract; use the current public 0.2.5 shim report.",
    "privatedocs/reports/readyos_shim_architecture_report_v3.html": "Historical 0.2.4 report with its full commented source appendix refreshed and byte-verified current. Other fixed-slot and ReadyShell measurements are superseded.",
    "privatedocs/reports/readyshell_command_overlay_experiment_2026-04-10.html": "Dated experiment record retained unchanged apart from this status notice; it is not the current overlay contract.",
    "privatedocs/reports/readyshell_overlay_bss_resume_headroom_0_1_8a.html": "Preserved 0.1.8A overlay/headroom measurement report; current values are in the generated overlay inventory.",
    "privatedocs/reports/readyshell_variable_memory_ram_reu_flow.html": "Preserved ReadyShell variable-flow analysis from the older 2246-byte-heap build. The current heap/map values are in the generated overlay inventory.",
    "privatedocs/reports/true_end_pipeline_design.html": "Design report retained for pipeline evolution context; implementation status must be checked against current source and tests.",
}


def banner(kind: str, message: str) -> str:
    current = kind == "current"
    bg = "#e7f7ec" if current else "#fff4d6"
    border = "#238636" if current else "#9a6700"
    label = "CURRENT DOCUMENT" if current else "PRESERVED SNAPSHOT / DESIGN"
    return (
        f"{START}\n"
        f'<aside role="note" data-readyos-doc-status="{kind}" '
        f'style="margin:16px;padding:12px 14px;border:2px solid {border};'
        f'background:{bg};color:#1f2328;font:14px/1.45 system-ui,sans-serif">'
        f"<strong>{label}</strong><br>{message}</aside>\n"
        f"{END}"
    )


def update(path_text: str, kind: str, message: str) -> None:
    path = ROOT / path_text
    if not path.exists():
        raise SystemExit(f"missing classified HTML document: {path_text}")
    text = path.read_text(encoding="utf-8")
    block = banner(kind, message)
    if START in text:
        before, rest = text.split(START, 1)
        _old, after = rest.split(END, 1)
        text = before + block + after
    else:
        body_end = text.find(">", text.lower().find("<body"))
        if body_end < 0:
            text = block + "\n" + text
        else:
            text = text[: body_end + 1] + "\n" + block + text[body_end + 1 :]
    path.write_text(text, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Update or verify ReadyOS documentation HTML status banners."
    )
    parser.add_argument(
        "--check", action="store_true",
        help="verify classification and exact banners without writing files",
    )
    args = parser.parse_args()
    classified = set(CURRENT) | set(HISTORICAL)
    actual = {
        str(path.relative_to(ROOT))
        for tree in (ROOT / "docs", ROOT / "privatedocs")
        for path in tree.rglob("*")
        if path.is_file() and path.suffix.lower() in {".html", ".htm"}
    }
    actual.update({
        "ReadyOSREUPhase1Completed.html",
        "src/apps/readybasic/readybasic_current_design.html",
        "src/apps/readybasic/readybasic_lifecycle_visual_guide.html",
        "src/apps/readybasic/readybasic_making_command_guide.html",
    })
    missing = sorted(actual - classified)
    extra = sorted(classified - actual)
    if missing or extra:
        raise SystemExit(
            "HTML classification mismatch\n"
            + ("unclassified: " + ", ".join(missing) + "\n" if missing else "")
            + ("missing: " + ", ".join(extra) if extra else "")
        )
    if args.check:
        failures = []
        for path_text, message in CURRENT.items():
            text = (ROOT / path_text).read_text(encoding="utf-8")
            if banner("current", message) not in text:
                failures.append(path_text)
        for path_text, message in HISTORICAL.items():
            text = (ROOT / path_text).read_text(encoding="utf-8")
            if banner("historical", message) not in text:
                failures.append(path_text)
        if failures:
            raise SystemExit("missing/stale status banner: " + ", ".join(failures))
        print(f"verified {len(classified)} classified HTML documents")
    else:
        for path, message in CURRENT.items():
            update(path, "current", message)
        for path, message in HISTORICAL.items():
            update(path, "historical", message)
        print(f"updated {len(classified)} classified HTML documents")


if __name__ == "__main__":
    main()
