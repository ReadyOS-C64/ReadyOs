#!/usr/bin/env python3
"""Render human/agent markdown from a machine stability report."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("report_json")
    ap.add_argument("--human-out")
    ap.add_argument("--agent-out")
    args = ap.parse_args()

    script_path = pathlib.Path(__file__).resolve()
    repo_root = script_path.parents[3]
    sys.path.insert(0, str(repo_root / "tools" / "stability"))

    from lib.report import render_agent_markdown, render_human_markdown  # pylint: disable=import-error

    report_path = pathlib.Path(args.report_json)
    if not report_path.is_absolute():
        report_path = (pathlib.Path.cwd() / report_path).resolve()

    report = json.loads(report_path.read_text(encoding="utf-8", errors="replace"))
    human = render_human_markdown(report)
    agent = render_agent_markdown(report)

    if args.human_out:
        pathlib.Path(args.human_out).write_text(human, encoding="utf-8")
    if args.agent_out:
        pathlib.Path(args.agent_out).write_text(agent, encoding="utf-8")

    if not args.human_out and not args.agent_out:
        print(human)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
