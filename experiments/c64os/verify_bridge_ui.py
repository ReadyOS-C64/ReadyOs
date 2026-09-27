#!/usr/bin/env python3
"""Drive the real launcher/bridge UI; verify destructive RAM round trips.

Run after experiments/c64os/run.sh, with its fresh launcher visible, or with
the bridge gate already open. This does not load or execute an app via monitor.
"""
import json
import os
from pathlib import Path
import re
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
ENV = Path(os.environ.get("READYOS_BRIDGE_ENV", ROOT.parent / "c64os-readyos-experiment")).resolve()
BINARY_PORT = int(os.environ.get("READYOS_BRIDGE_BINARY_PORT", "6511"))
TEXT_PORT = int(os.environ.get("READYOS_BRIDGE_TEXT_PORT", "6611"))
sys.path.insert(0, str(ROOT.parent / "c64os-tasks/tools"))
from vice_harness import Monitor


def screen(mon):
    raw = mon.read(0x400, 1000)
    lines = []
    for row in range(25):
        chars = [c & 127 for c in raw[row*40:row*40+40]]
        lines.append("".join(chr(c + 64 if c < 32 else c) for c in chars))
    return "\n".join(lines)


def wait_screen(mon, expected, timeout=30):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        result = screen(mon)
        mon.command(0xAA)
        if expected in result:
            return result
        time.sleep(.1)
    raise RuntimeError(f"UI did not show {expected!r}:\n{result}")


def feed(mon, keys):
    mon.command(0x72, bytes([len(keys)]) + bytes(keys))
    mon.command(0xAA)


def screenshot(name):
    sys.path.insert(0, str(ROOT.parent / "agenticdevharness/tools"))
    from vice_readyshell_automation import Monitor as TextMonitor
    path = ENV / name
    mon = TextMonitor("127.0.0.1", TEXT_PORT)
    try:
        mon.cmd(f'screenshot "{path}" 2\n')
        mon.cmd("x\n")
    finally:
        mon.close()
    assert path.is_file() and path.stat().st_size > 0
    return str(path)


def main():
    with socket.create_connection(("127.0.0.1", BINARY_PORT), 5) as sock:
        sock.settimeout(10)
        mon = Monitor(sock)
        text = screen(mon)
        mon.command(0xAA)
        if "START / RESUME C64OS (C)" not in text:
            wait_screen(mon, "APPLICATIONS")
            feed(mon, [19,17,17,13])
        text = wait_screen(mon, "START / RESUME C64OS (C)")
        assert "REQUIRES SKIP" not in text
        previous = [int(n) for n in re.findall(r"RESTORED ZP/STACK/SHIM: (\d+)", text)]
        count = max(previous, default=0)
        shim = mon.read(0xc600, 1024)
        assert shim[0x23b] == 39
        mon.command(0xAA)
        for expected in range(count+1, count+4):
            feed(mon, b"S")
            text = wait_screen(mon, f"RESTORED ZP/STACK/SHIM: {expected}")
            assert mon.read(0xc600, 1024) == shim, "Entire resident shim changed"
            mon.command(0xAA)
        (ENV / "bridge-proof-screen.txt").write_text(text + "\n")
        proof = screenshot("bridge-proof.png")
        feed(mon, [133])
        launcher = wait_screen(mon, "APPLICATIONS")
        assert mon.read(0xc83b, 1) == b"\x27"
        mon.command(0xAA)
    shot = screenshot("bridge-proof-return.png")
    report = {"pass": True, "cycles_this_run": 3, "cycles_displayed": count+3,
              "entire_shim_bytes_compared_each_cycle": 1024,
              "returned_through_shim_to_launcher": True,
              "screenshots": [proof, shot],
              "scope": "Same-context full-RAM transfer gate; not yet a C64 OS handoff"}
    (ENV / "bridge-proof.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
