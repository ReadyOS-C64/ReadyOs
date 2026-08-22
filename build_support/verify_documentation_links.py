#!/usr/bin/env python3
"""Verify local links in every tracked ReadyOS Markdown and HTML document."""

from __future__ import annotations

import html.parser
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit


ROOT = Path(__file__).resolve().parents[1]
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")


class LinkParser(html.parser.HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.links: list[str] = []

    def handle_starttag(self, _tag: str, attrs: list[tuple[str, str | None]]) -> None:
        for name, value in attrs:
            if name.lower() in {"href", "src"} and value:
                self.links.append(value)


def tracked_documents() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-z", "*.md", "*.html", "*.htm"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [ROOT / raw.decode("utf-8") for raw in result.stdout.split(b"\0") if raw]


def link_target(raw: str) -> str | None:
    raw = raw.strip()
    if raw.startswith("<") and raw.endswith(">"):
        raw = raw[1:-1]
    # Markdown permits an optional quoted title after the destination.
    raw = re.split(r'\s+["\']', raw, maxsplit=1)[0]
    if not raw or raw.startswith("#") or "{{" in raw:
        return None
    parsed = urlsplit(raw)
    if parsed.scheme or parsed.netloc:
        return None
    return unquote(parsed.path) or None


def links_from(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8", errors="replace")
    if path.suffix.lower() == ".md":
        return [match.group(1) for match in MARKDOWN_LINK.finditer(text)]
    parser = LinkParser()
    parser.feed(text)
    return parser.links


def main() -> int:
    failures: list[str] = []
    checked_links = 0
    documents = tracked_documents()
    for path in documents:
        for raw in links_from(path):
            target_text = link_target(raw)
            if target_text is None:
                continue
            checked_links += 1
            target = (path.parent / target_text).resolve()
            try:
                target.relative_to(ROOT)
            except ValueError:
                failures.append(f"{path.relative_to(ROOT)}: link escapes repository: {raw}")
                continue
            if not target.exists():
                failures.append(f"{path.relative_to(ROOT)}: missing local target: {raw}")

    if failures:
        print("DOCUMENTATION LINK VERIFICATION FAILED")
        for failure in failures:
            print(f"  [FAIL] {failure}")
        return 1
    print(
        "DOCUMENTATION LINK VERIFICATION PASSED: "
        f"{len(documents)} tracked documents, {checked_links} local links"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
