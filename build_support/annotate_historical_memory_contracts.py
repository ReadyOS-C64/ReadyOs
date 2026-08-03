#!/usr/bin/env python3
"""Add a repeat-safe current-contract banner to retained historical reports.

The reports below intentionally keep their dated measurements and proposals.
This script adds context without replacing or deleting the historical body.
"""

from __future__ import annotations

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MARKER = "READYOS-CURRENT-CONTRACT-2026-08-02"

MARKDOWN_PATHS = (
    "ReadyOSREUPhase1Completed.md",
    "futureREUrefactor.md",
    "reurefactorlessonslearnt.md",
    "privatedocs/reports/OLD_READYBASIC_MEMORY_REARRANGEMENT_IMPLEMENTED.md",
    "privatedocs/reports/easyflash_boot_flow.md",
    "privatedocs/reports/potential_future_readyos_ram_maximization_plan.md",
    "privatedocs/reports/readybasic_design_ideas.md",
    "privatedocs/reports/readyos_all_apps_memory_contract_compare.md",
    "privatedocs/reports/readyshell_variable_memory_ram_reu_flow.md",
    "privatedocs/research/architecture.md",
    "privatedocs/research/dizzy_verification_matrix.md",
    "privatedocs/research/readyos_c64os_interop_design.md",
    "privatedocs/research/READYSHELL_EDITOR_OVERLAY_REFACTOR_NOTES.md",
    "privatedocs/top_level_md/ARCHITECTURE_PLAN.md",
    "privatedocs/top_level_md/IMPLEMENTATION_PLAN.md",
)

HTML_PATHS = (
    "ReadyOSREUPhase1Completed.html",
    "docs/reports/ReadyOSREUPhase1Completed.html",
    "docs/reports/readyos_memory_size_comparison_0_1_5_vs_now.html",
    "docs/reports/simplefiles_once_bss_resume_headroom_0_1_8w.html",
    "privatedocs/reports/easyflash_boot_flow.html",
    "privatedocs/reports/old_readybasic_memory_rearrangement_implemented.html",
    "privatedocs/reports/potential_future_readyos_ram_maximization_plan.html",
    "privatedocs/reports/readybasic_design_ideas.html",
    "privatedocs/reports/readyos_c64os_interop_design.html",
    "privatedocs/reports/readyos_c64os_interop_design_v2.html",
    "privatedocs/reports/readyos_memory_layout_print.html",
    "privatedocs/reports/readyos_memory_size_comparison_0_1_5_vs_now.html",
    "privatedocs/reports/readyos_shim_architecture_report.html",
    "privatedocs/reports/readyos_shim_architecture_report_v2.html",
    "privatedocs/reports/readyos_shim_architecture_report_v3 copy.html",
    "privatedocs/reports/readyshell_overlay_bss_resume_headroom_0_1_8a.html",
)

MD_NOTE = f"""
<!-- {MARKER} -->
> **Current ReadyOS contract (2026-08-02):** Physical `Skip` is the ReadyOS
> bank and `Skip+1` is the first dynamic bank. The launcher snapshot occupies
> ReadyOS `$0000-$B5FF`; schema v5 occupies `$B600-$FFFF`, including the token
> map at `$B740` and status at `$B840`. C64 app RAM is `$1000-$C5FF` (`$B600`),
> and the resident 1 KB shim owns `$C600-$C9FF` with its public ABI at `$C800`.
> Dated layouts and measurements below are retained as historical evidence.
""".strip()

HTML_NOTE = f"""
<!-- {MARKER} -->
<aside style="margin:1rem auto;padding:1rem;border:2px solid #d69e2e;background:#fffaf0;color:#3d2b00;max-width:72rem">
  <strong>Current ReadyOS contract (2026-08-02):</strong>
  Physical <code>Skip</code> is the ReadyOS bank and <code>Skip+1</code> is the first dynamic bank.
  The launcher snapshot is ReadyOS <code>$0000-$B5FF</code>; schema v5 is <code>$B600-$FFFF</code>,
  with token map <code>$B740</code> and status <code>$B840</code>. C64 app RAM is
  <code>$1000-$C5FF</code> (<code>$B600</code>), and the resident 1 KB shim owns
  <code>$C600-$C9FF</code> with its public ABI at <code>$C800</code>.
  Dated layouts and measurements below are retained as historical evidence.
</aside>
""".strip()


def annotate_markdown(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        return False
    match = re.search(r"(?m)^# .+\n", text)
    if not match:
        raise SystemExit(f"missing top-level Markdown heading: {path}")
    updated = text[: match.end()] + "\n" + MD_NOTE + "\n" + text[match.end():]
    path.write_text(updated, encoding="utf-8")
    return True


def annotate_html(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    if MARKER in text:
        return False
    match = re.search(r"<body(?:\s[^>]*)?>", text, re.IGNORECASE)
    if not match:
        updated = HTML_NOTE + "\n" + text
    else:
        updated = text[: match.end()] + "\n" + HTML_NOTE + "\n" + text[match.end():]
    path.write_text(updated, encoding="utf-8")
    return True


def main() -> None:
    changed: list[Path] = []
    for rel in MARKDOWN_PATHS:
        path = ROOT / rel
        if annotate_markdown(path):
            changed.append(path)
    for rel in HTML_PATHS:
        path = ROOT / rel
        if annotate_html(path):
            changed.append(path)
    print(f"annotated {len(changed)} retained historical reports")
    for path in changed:
        print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
