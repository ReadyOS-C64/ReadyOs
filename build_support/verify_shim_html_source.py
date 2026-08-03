#!/usr/bin/env python3
"""Verify every HTML full-shim listing against the canonical byte image.

Styled reports may add explanatory comments, so this checker compares every
`.byte` directive (including directive boundaries and operands) and separately
requires the current control-bank lookup annotations.
"""

from html.parser import HTMLParser
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src/boot/readyos_shim.inc"
MARKER = "Shared ReadyOS shim image ($C600-$C9FF, 1024 bytes)"

REQUIRED_ANNOTATIONS = (
    "$C83B: readyos_bank",
    "Resolve token via the ReadyOS bank",
    "lookup_app_bank",
    "ADC #<$B740",
    "fetch physical bank byte",
    "Padding to $C9A0",
    "mark_loaded",
    "len hi = $B6",
)

RETIRED_ANNOTATIONS = (
    "$C83D-$C83F: reserved",
    "BNE app_bank",
    "JMP store_physical_bank",
    "global + launcher overlay reserve",
    "Start+3..Start+25: physical backing banks",
    "Resolve logical token via REU bank 0",
    "ADC #<$2F00",
)


class PreBlocks(HTMLParser):
    def __init__(self) -> None:
        super().__init__()
        self.in_pre = False
        self.current = []
        self.blocks = []

    def handle_starttag(self, tag, attrs) -> None:
        if tag == "pre":
            self.in_pre = True
            self.current = []

    def handle_endtag(self, tag) -> None:
        if tag == "pre" and self.in_pre:
            self.blocks.append("".join(self.current))
            self.current = []
            self.in_pre = False

    def handle_data(self, data) -> None:
        if self.in_pre:
            self.current.append(data)


def byte_directives(text: str) -> list[str]:
    result = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped.startswith(".byte"):
            continue
        operation = stripped.split(";", 1)[0].strip()
        result.append(re.sub(r"\s+", " ", operation))
    return result


def main() -> None:
    canonical = byte_directives(SOURCE.read_text(encoding="utf-8"))
    checked = []
    failures = []

    for tree_name in ("docs", "privatedocs"):
        for path in (ROOT / tree_name).rglob("*"):
            if not path.is_file() or path.suffix.lower() not in {".html", ".htm"}:
                continue
            parser = PreBlocks()
            parser.feed(path.read_text(encoding="utf-8"))
            for block_index, block in enumerate(parser.blocks, start=1):
                if MARKER not in block:
                    continue
                label = f"{path.relative_to(ROOT)} pre#{block_index}"
                checked.append(label)
                embedded = byte_directives(block)
                if embedded != canonical:
                    mismatch = next(
                        (
                            index
                            for index, pair in enumerate(zip(canonical, embedded), start=1)
                            if pair[0] != pair[1]
                        ),
                        min(len(canonical), len(embedded)) + 1,
                    )
                    failures.append(
                        f"{label}: byte directives differ at #{mismatch} "
                        f"(canonical {len(canonical)}, embedded {len(embedded)})"
                    )
                for annotation in REQUIRED_ANNOTATIONS:
                    if annotation not in block:
                        failures.append(f"{label}: missing annotation: {annotation}")
                for annotation in RETIRED_ANNOTATIONS:
                    if annotation in block:
                        failures.append(f"{label}: retired annotation remains: {annotation}")

    if not checked:
        raise SystemExit("no full commented shim listing found in documentation HTML")
    if failures:
        raise SystemExit("\n".join(failures))

    print(
        f"verified {len(checked)} full HTML shim listings: "
        f"{len(canonical)} canonical .byte directives each"
    )


if __name__ == "__main__":
    main()
