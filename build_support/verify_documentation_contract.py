#!/usr/bin/env python3
"""Verify the live documentation against the schema-v5 memory contract.

Historical reports deliberately retain their original measurements, so this
gate checks their supersession markers while applying exact current-value
checks only to documents that claim to describe the live tree.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import annotate_historical_memory_contracts as historical


ROOT = Path(__file__).resolve().parents[1]

REQUIRED: dict[str, tuple[str, ...]] = {
    "README.md": (
        "app runtime window: `$1000-$C5FF` (`$B600` bytes)",
        "resident shim: `$C600-$C9FF` (1 KB)",
        "physical `Skip`: the ReadyOS bank",
        "physical `Skip+1` and above: dynamic allocation pool",
    ),
    "Releases/0.2.5/README.md": (
        "active app snapshot is `$1000-$C5FF` (`$B600` bytes)",
        "Physical REU bank `Skip` is the **ReadyOS bank**",
        "Physical `Skip+1` is the first dynamic bank",
    ),
    "docs/release_root_readme_template.md": (
        "active app snapshot is `$1000-$C5FF` (`$B600` bytes)",
        "Physical REU bank `Skip` is the **ReadyOS bank**",
    ),
    "docs/ReadyOS_SHIM_ARCHITECTURE_0.2.5.md": (
        "Physical `Skip` is the ReadyOS bank",
        "Physical `Skip+1` is the first dynamic app/resource bank",
        "full shim region is restored to 1 KB at `$C600-$C9FF`",
        "ReadyOS:$B740 + token",
        "token validity/loaded/resumable state | ReadyOS `$B840-$B93F`",
    ),
    "docs/ReadyOS_SHIM_ARCHITECTURE_0.2.5.html": (
        "Physical <code>Skip</code> is the ReadyOS bank",
        "<code>$C600-$C9FF</code>",
        "<code>ReadyOS:$B740 + token</code>",
    ),
    "docs/ultimate_dos_dma_loading.md": (
        "`$1000-$C5FF`",
        "`$B600` REU\nstash/fetch operations",
        "exact-size `LOAD_REU`",
    ),
    "docs/ultimate_dos_dma_loading.html": (
        "<code>$1000-$C5FF</code>",
        "<code>$B600</code> REU stash/fetch",
        "exact-size <code>LOAD_REU</code>",
    ),
    "docs/reports/easyflash_boot_flow.md": (
        "full app window `$1000-$C5FF` (`$B600` bytes)",
        "full `46,592` byte app window",
        "Physical `Skip` is the\nsingle source of truth",
    ),
    "docs/reports/easyflash_boot_flow.html": (
        "full <code>46,592</code> byte app window",
        "Physical\n<code>Skip</code> is the single source of truth",
    ),
    "privatedocs/top_level_md/MEMORY_MAP.md": (
        "Skip (ReadyOS bank)",
        "`$B940` | `$0090` | clipboard count plus 16 item records",
        "clipboard metadata lives in ReadyOS `$B940-$B9CF`",
        "agentworking/readyos-1kb-shim-skip-bank/headroom_comparison.md",
    ),
    "privatedocs/top_level_md/SHIM_PLAN.md": (
        "`Skip` | ReadyOS bank: launcher snapshot + `RCB5` schema 5",
        "`Skip+1..detected end` | dynamic pool",
        "`$C600-$C9FF`; `$C600-$C7FF` is resident expansion reserve",
    ),
    "src/apps/readybasic/readyBASICrefactorguidelines.md": (
        "`$C600-$C7FF` | Reserved resident ReadyOS shim expansion capacity",
    ),
    "src/apps/readyshell/README.md": (
        "ReadyOS snapshot window: `$1000-$C5FF` (`46592` bytes)",
        "Overlay execution window: `$8E00-$C5FF` (`14336` bytes)",
    ),
    "Releases/0.2.5/precog-kung-fu-flash-2-d81/README.md": (
        "Fresh launcher state uses `1` bank by default",
        "That leaves `15` banks",
        "about `6/16` banks in use including the ReadyOS bank",
        "about `4/16` banks in use including the ReadyOS bank",
        "about `9/16` banks including the ReadyOS bank",
    ),
}

FORBIDDEN: dict[str, tuple[str, ...]] = {
    "docs/ultimate_dos_dma_loading.md": ("`$B800` REU\nstash/fetch operations",),
    "docs/ultimate_dos_dma_loading.html": ("<code>$B800</code> REU stash/fetch",),
    "docs/reports/easyflash_boot_flow.md": ("full `47,104` byte app window",),
    "docs/reports/easyflash_boot_flow.html": ("full <code>47,104</code> byte app window",),
    "privatedocs/top_level_md/MEMORY_MAP.md": (
        "clipboard metadata lives in ReadyOS `$BB40-$BBCF`",
        "agentworking/readyos-bank-refactor/headroom_comparison.md",
    ),
    "Releases/0.2.5/precog-kung-fu-flash-2-d81/README.md": (
        "Fresh launcher state uses `2` banks by default",
        "That leaves `14` banks",
        "about `7/16` banks in use",
        "about `5/16` banks in use",
        "about `10/16` banks",
    ),
    "build_support/readyos_profiles.py": (
        "Fresh launcher state uses `2` banks by default",
        "That leaves `14` banks",
    ),
}


def main() -> int:
    failures: list[str] = []
    checked = 0
    for rel, snippets in REQUIRED.items():
        path = ROOT / rel
        if not path.exists():
            failures.append(f"missing current document: {rel}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        checked += 1
        for snippet in snippets:
            if snippet not in text:
                failures.append(f"{rel}: missing current contract text {snippet!r}")

    for rel, snippets in FORBIDDEN.items():
        path = ROOT / rel
        if not path.exists():
            failures.append(f"missing checked file: {rel}")
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for snippet in snippets:
            if snippet in text:
                failures.append(f"{rel}: stale current-contract text {snippet!r}")

    for rel in (*historical.MARKDOWN_PATHS, *historical.HTML_PATHS):
        path = ROOT / rel
        if not path.exists():
            failures.append(f"missing retained historical document: {rel}")
            continue
        if historical.MARKER not in path.read_text(encoding="utf-8", errors="replace"):
            failures.append(f"{rel}: missing current-contract supersession marker")

    status = subprocess.run(
        [sys.executable, str(ROOT / "build_support/update_documentation_html_status.py"), "--check"],
        cwd=ROOT,
        text=True,
        capture_output=True,
    )
    if status.returncode != 0:
        failures.append("HTML classification check failed: " + (status.stderr or status.stdout).strip())

    if failures:
        print("DOCUMENTATION CONTRACT VERIFICATION FAILED")
        for failure in failures:
            print(f"  [FAIL] {failure}")
        return 1

    print(
        "DOCUMENTATION CONTRACT VERIFICATION PASSED: "
        f"{checked} current documents, "
        f"{len(historical.MARKDOWN_PATHS) + len(historical.HTML_PATHS)} historical markers"
    )
    if status.stdout.strip():
        print(status.stdout.strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
