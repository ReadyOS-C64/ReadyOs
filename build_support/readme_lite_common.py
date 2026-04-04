#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass
from typing import List, Sequence, Tuple

STYLE_NORMAL = 0
STYLE_H1 = 1
STYLE_H2 = 2
STYLE_H3 = 3
STYLE_BOLD = 4
STYLE_ITALIC = 5


@dataclass
class StyledLine:
    text: str
    styles: List[int]


def _parse_inline(text: str, base_style: int) -> Tuple[List[str], List[int]]:
    chars: List[str] = []
    styles: List[int] = []
    i = 0
    bold = False
    italic = False

    while i < len(text):
        if text.startswith("**", i):
            bold = not bold
            i += 2
            continue
        if text[i] == "*":
            italic = not italic
            i += 1
            continue

        ch = text[i]
        if bold:
            style = STYLE_BOLD
        elif italic:
            style = STYLE_ITALIC
        else:
            style = base_style

        chars.append(ch)
        styles.append(style)
        i += 1

    return chars, styles


def _wrap_styled(chars: Sequence[str], styles: Sequence[int], width: int) -> List[StyledLine]:
    out: List[StyledLine] = []
    n = len(chars)
    start = 0

    while start < n:
        while start < n and chars[start] == " ":
            start += 1
        if start >= n:
            break

        max_end = min(start + width, n)
        end = max_end

        if max_end < n:
            split = -1
            p = max_end - 1
            while p > start:
                if chars[p] == " ":
                    split = p
                    break
                p -= 1
            if split != -1:
                end = split

        seg_chars = list(chars[start:end])
        seg_styles = list(styles[start:end])
        out.append(StyledLine("".join(seg_chars), seg_styles))

        start = end

    if not out:
        out.append(StyledLine("", []))

    return out


def parse_markdown_lite(content: str, width: int, lines_per_page: int) -> List[List[StyledLine]]:
    token_lines: List[StyledLine | None] = []

    for raw_line in content.splitlines():
        line = raw_line.rstrip()
        stripped = line.strip()

        if stripped == "---":
            token_lines.append(None)
            continue

        if stripped == "":
            token_lines.append(StyledLine("", []))
            continue

        if stripped.startswith("### "):
            chars, styles = _parse_inline(stripped[4:], STYLE_H3)
            token_lines.extend(_wrap_styled(chars, styles, width))
            continue

        if stripped.startswith("## "):
            chars, styles = _parse_inline(stripped[3:], STYLE_H2)
            token_lines.extend(_wrap_styled(chars, styles, width))
            continue

        if stripped.startswith("# "):
            chars, styles = _parse_inline(stripped[2:], STYLE_H1)
            token_lines.extend(_wrap_styled(chars, styles, width))
            continue

        if stripped.startswith("- "):
            bullet_text = stripped[2:]
            body_chars, body_styles = _parse_inline(bullet_text, STYLE_NORMAL)
            wrapped = _wrap_styled(body_chars, body_styles, width - 2)
            for idx, segment in enumerate(wrapped):
                if idx == 0:
                    seg_text = "* " + segment.text
                    seg_styles = [STYLE_NORMAL, STYLE_NORMAL] + segment.styles
                else:
                    seg_text = "  " + segment.text
                    seg_styles = [STYLE_NORMAL, STYLE_NORMAL] + segment.styles
                token_lines.append(StyledLine(seg_text, seg_styles))
            continue

        chars, styles = _parse_inline(stripped, STYLE_NORMAL)
        token_lines.extend(_wrap_styled(chars, styles, width))

    pages: List[List[StyledLine]] = [[]]

    for token in token_lines:
        page = pages[-1]

        if token is None:
            while len(page) < lines_per_page:
                page.append(StyledLine("", []))
            pages.append([])
            continue

        if len(page) >= lines_per_page:
            pages.append([])
            page = pages[-1]

        page.append(token)

    if not pages[-1]:
        pages.pop()

    if not pages:
        pages = [[]]

    for page in pages:
        while len(page) < lines_per_page:
            page.append(StyledLine("", []))

    return pages
