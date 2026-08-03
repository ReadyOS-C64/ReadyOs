#!/usr/bin/env python3
"""Refresh every complete Markdown/HTML shim appendix from the canonical source.

Historical reports are intentionally preserved, but an appendix claiming to
contain the current complete shim must be exact.  This script changes only the
marked code block/preformatted block and leaves the surrounding report intact.
"""

from __future__ import annotations

import html
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/boot/readyos_shim.inc"
MARKERS = (
    "Shared ReadyOS shim image ($C600-$C9FF, 1024 bytes)",
    "Shared ReadyOS shim image ($C800-$C9FF, 512 bytes)",
)


def marker_offset(text: str) -> int:
    offsets = [text.index(marker) for marker in MARKERS if marker in text]
    return min(offsets) if offsets else -1


def sync_markdown(path: Path, source: str) -> bool:
    text = path.read_text(encoding="utf-8")
    marker = marker_offset(text)
    if marker < 0:
        return False
    opening = text.rfind("```", 0, marker)
    closing = text.find("```", marker)
    if opening < 0 or closing < 0:
        raise SystemExit(f"marked shim listing is not fenced: {path}")
    opening_end = text.find("\n", opening)
    updated = text[: opening_end + 1] + source.rstrip() + "\n" + text[closing:]
    path.write_text(updated, encoding="utf-8")
    return True


def sync_html(path: Path, source: str) -> bool:
    text = path.read_text(encoding="utf-8")
    marker = marker_offset(text)
    if marker < 0:
        return False
    opening = text.rfind("<pre", 0, marker)
    opening_end = text.find(">", opening)
    closing = text.find("</pre>", marker)
    if opening < 0 or opening_end < 0 or closing < 0:
        raise SystemExit(f"marked shim listing is not in a pre block: {path}")
    escaped = html.escape(source.rstrip(), quote=False)
    updated = text[: opening_end + 1] + escaped + "\n" + text[closing:]
    path.write_text(updated, encoding="utf-8")
    return True


def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    changed = []
    for path in ROOT.rglob("*"):
        if not path.is_file() or ".git" in path.parts or "third_party" in path.parts:
            continue
        if path.suffix.lower() == ".md" and sync_markdown(path, source):
            changed.append(path)
        elif path.suffix.lower() in {".html", ".htm"} and sync_html(path, source):
            changed.append(path)
    print(f"refreshed {len(changed)} complete shim appendices")
    for path in changed:
        print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
