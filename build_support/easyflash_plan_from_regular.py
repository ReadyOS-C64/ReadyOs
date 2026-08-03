#!/usr/bin/env python3
"""Convert a regular ReadyOS VICE plan into an EasyFlash launcher-driven plan."""

from __future__ import annotations

import argparse
from pathlib import Path


READYBASIC_NAV = [17, 17, 17, 17, 13]
READYSHELL_NAV = [17, 13]
EDITOR_NAV = [13]


def nav_keys(start_app: str) -> list[int]:
    if start_app == "readybasic":
        return READYBASIC_NAV
    if start_app == "readyshell":
        return READYSHELL_NAV
    if start_app == "editor":
        return EDITOR_NAV
    raise ValueError(f"unsupported start app: {start_app}")


def replace_plan_id(text: str, suffix: str) -> str:
    lines = text.splitlines()
    for i, line in enumerate(lines):
        if line.startswith("plan_id: "):
            current = line.split(":", 1)[1].strip()
            lines[i] = f"plan_id: {current}{suffix}"
            break
    return "\n".join(lines) + "\n"


def replace_vice_block(text: str, crt: str, disk8: str) -> str:
    lines = text.splitlines()
    vice_idx = None
    steps_idx = None
    for i, line in enumerate(lines):
        if line == "  vice:" and vice_idx is None:
            vice_idx = i
        if line == "steps:":
            steps_idx = i
            break
    if vice_idx is None or steps_idx is None or vice_idx > steps_idx:
        raise ValueError("could not locate global_defaults vice block")

    block = [
        "  vice:",
        f'    cart_crt: "{crt}"',
        "    autostart_enabled: false",
        f'    disk8: "{disk8}"',
        "    drive8_enabled: true",
        "    drive8_type: 1541",
        "    drive9_enabled: false",
        "    true_drive: true",
        "    close_vice: true",
        "    headless: true",
        "    speed_percent: 100",
    ]
    replaced = lines[:vice_idx] + block + lines[steps_idx:]
    return "\n".join(replaced) + "\n"


def insert_launcher_start(text: str, start_app: str | None) -> str:
    if not start_app:
        return text

    lines = text.splitlines()
    insert_idx = None
    for i, line in enumerate(lines):
        if line.strip() == "kill_stale: true":
            insert_idx = i + 1
            break
    if insert_idx is None:
        raise ValueError("could not find initial vice.launch kill_stale step")

    keys = ",".join(str(k) for k in nav_keys(start_app))
    injected = [
        "  - id: easyflash_wait_launcher",
        "    type: screen.wait_contains",
        "    params:",
        '      text: "READY OS"',
        "      pre_delay_s: 4",
        "      poll_s: 0.5",
        "      wait_timeout_s: 240",
        f"      capture_label: easyflash_{start_app}_launcher",
        f"  - id: easyflash_start_{start_app}",
        "    type: input.sequence",
        "    params:",
        f"      keys: [{keys}]",
        "      inter_key_delay_s: 0.08",
        "      post_delay_s: 2.5",
    ]
    return "\n".join(lines[:insert_idx] + injected + lines[insert_idx:]) + "\n"


def adapt_screen_reu_probe(text: str) -> str:
    """Allow extra cartridge settling time for the screen/REU stress probe.

    The regular plan carries the exact-RAM assertion too: the restored
    ASCII-shaped screen bytes render correctly in VICE but are not decoded
    as text by the harness screen decoder.
    """
    if "plan_id: readybasic_screen_reu_temp_probe_easyflash" not in text:
        return text

    text = text.replace("      post_delay_s: 30.0", "      post_delay_s: 75.0", 1)
    return text


def adapt_readybasic_reuviewer_chain(text: str) -> str:
    if "plan_id: readybasic_reuviewer_f2_chain_probe_easyflash" not in text:
        return text

    old = """  - id: launch_readybasic_from_launcher
    type: input.sequence
    params:
      keys: [17,17,17,17,17,17,13]
"""
    new = """  - id: launch_readybasic_from_launcher
    type: input.sequence
    params:
      keys: [17,17,17,17,13]
"""
    if old not in text:
        raise ValueError("could not locate ReadyBASIC launcher navigation in chain probe")
    return text.replace(old, new, 1)


def harden_readybasic_prompt_waits(text: str) -> str:
    """Avoid matching the ReadyBASIC title before the BASIC prompt is ready."""
    lines = text.splitlines()
    in_readybasic_wait = False
    for i, line in enumerate(lines):
        stripped = line.strip()
        if line.startswith("  - id: "):
            step_id = line.split(":", 1)[1].strip()
            in_readybasic_wait = "wait_readybasic" in step_id or step_id == "wait_first_readybasic_prompt"
            continue
        if in_readybasic_wait and stripped in ('text: "readybasic"', 'text: "READYBASIC"'):
            lines[i] = line[: len(line) - len(line.lstrip())] + 'text: "READY."'
            in_readybasic_wait = False
    return "\n".join(lines) + "\n"


def harden_easyflash_launcher_waits(text: str) -> str:
    """Give cartridge preload enough time to reach the launcher.

    A retry of the wait step observes the same already-stuck cartridge boot.
    The aggregate runner retries the complete plan with a fresh VICE process,
    so keep one full readiness window here and preserve functional step retry
    policy everywhere else.
    """
    lines = text.splitlines()
    output = []
    in_launcher_wait = False
    saw_pre_delay = False
    saw_poll = False
    for line in lines:
        stripped = line.strip()
        if line.startswith("  - id: "):
            step_id = line.split(":", 1)[1].strip()
            in_launcher_wait = step_id in ("easyflash_wait_launcher", "wait_launcher_initial")
            saw_pre_delay = False
            saw_poll = False
            output.append(line)
            if in_launcher_wait:
                output.extend((
                    "    retry:",
                    "      max_attempts: 1",
                    "      backoff_ms: 0",
                    "      jitter: false",
                ))
            continue
        if in_launcher_wait and stripped.startswith("pre_delay_s: "):
            saw_pre_delay = True
        if in_launcher_wait and stripped.startswith("poll_s: "):
            saw_poll = True
        if in_launcher_wait and stripped.startswith("wait_timeout_s: "):
            indent = line[: len(line) - len(line.lstrip())]
            current = int(stripped.split(":", 1)[1].strip())
            if not saw_pre_delay:
                output.append(f"{indent}pre_delay_s: 4")
            if not saw_poll:
                output.append(f"{indent}poll_s: 0.5")
            output.append(f"{indent}wait_timeout_s: {max(current, 420)}")
            in_launcher_wait = False
            continue
        output.append(line)
    return "\n".join(output) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--crt", required=True)
    parser.add_argument("--disk8", required=True)
    parser.add_argument(
        "--start-app",
        choices=("editor", "readybasic", "readyshell"),
    )
    parser.add_argument("--plan-id-suffix", default="_easyflash")
    args = parser.parse_args()

    text = args.input.read_text(encoding="utf-8")
    text = replace_plan_id(text, args.plan_id_suffix)
    text = replace_vice_block(text, args.crt, args.disk8)
    text = insert_launcher_start(text, args.start_app)
    text = adapt_screen_reu_probe(text)
    text = adapt_readybasic_reuviewer_chain(text)
    text = harden_readybasic_prompt_waits(text)
    text = harden_easyflash_launcher_waits(text)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
