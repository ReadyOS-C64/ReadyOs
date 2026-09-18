#!/usr/bin/env python3
"""Render current editorial Markdown; historical HTML bodies are never replaced."""
import argparse
import os
from pathlib import Path
import subprocess

from update_documentation_html_status import CURRENT, banner

ROOT = Path(__file__).resolve().parents[1]
PAIRS = {
    **{f"docs/{name}.md": f"docs/{name}.html" for name in (
        "DOCUMENTATION_INDEX", "ultimate_setup", "ultimate_dos_dma_loading",
        "readybasic_reference", "app_requirements", "documentation_audit_2026-09",
        "readybasic_orbital_echoes", "readybasic_media_learnings",
        "readybasic_media_load_investigation", "readyos_ultimate_speed_policy")},
    "src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md": "src/apps/readybasic/readybasic_current_design.html",
    "src/apps/readybasic/READYBASIC_LIFECYCLE_AND_REU_ARCHITECTURE.md": "src/apps/readybasic/readybasic_lifecycle_visual_guide.html",
    "src/apps/readybasic/READYBASIC_MAKING_COMMAND_GUIDE.md": "src/apps/readybasic/readybasic_making_command_guide.html",
    "privatedocs/reports/readybasic_current_state.md": "privatedocs/reports/readybasic_current_state.html",
}

def render(source, target):
    md, out = ROOT / source, ROOT / target
    title = next(line.lstrip("# ") for line in md.read_text().splitlines() if line.startswith("# "))
    css = os.path.relpath(ROOT / "docs/readyos_docs.css", out.parent)
    result = subprocess.run([
        "pandoc", str(md), "--from=markdown-tex_math_dollars-smart", "--to=html5", "--standalone",
        "--toc", "--toc-depth=2", "--section-divs", "--metadata", f"pagetitle={title}",
        "--css", css, "--wrap=none",
    ], check=True, capture_output=True, text=True)
    html = result.stdout
    # A 45-example reference needs navigation, but not several screens of TOC
    # before its introduction. Native details keeps it accessible and offline.
    html = html.replace('<nav id="TOC"', '<details class="contents"><summary>Contents — commands, modules and examples</summary><nav id="TOC"')
    html = html.replace('</nav>', '</nav></details>', 1)
    body_end = html.index(">", html.index("<body")) + 1
    html = html[:body_end] + "\n" + banner("current", CURRENT[target]) + html[body_end:]
    # Keep navigation in the styled edition when a counterpart is rendered here.
    for other_md, other_html in PAIRS.items():
        rel_md = os.path.relpath(ROOT / other_md, md.parent)
        rel_html = os.path.relpath(ROOT / other_html, out.parent)
        html = html.replace(f'href="{rel_md}"', f'href="{rel_html}"')
    return html

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    stale = []
    checked = 0
    for source, target in PAIRS.items():
        # Private sources are optional in a public checkout; never publish them.
        if source.startswith("privatedocs/") and not (ROOT / source).exists():
            continue
        html = render(source, target)
        checked += 1
        path = ROOT / target
        if args.check:
            if not path.exists() or path.read_text() != html:
                stale.append(target)
        else:
            path.write_text(html)
            print(target)
    if stale:
        raise SystemExit("Stale HTML: " + ", ".join(stale))
    if args.check:
        print(f"Verified {checked} Markdown/HTML counterparts")

if __name__ == "__main__":
    main()
