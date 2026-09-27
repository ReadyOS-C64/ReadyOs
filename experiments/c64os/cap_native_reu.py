#!/usr/bin/env python3
"""Prepare the private HDD using native IDEDOS LOAD/SAVE through a live VICE.

Run only with the experiment's disposable runtime HDD attached, at BASIC.
This is installation tooling; the bridge must never depend on a host monitor.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import socket
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
ENV = Path(os.environ.get("READYOS_BRIDGE_ENV", ROOT.parent / "c64os-readyos-experiment")).resolve()
sys.path.insert(0, str(ROOT.parent / "c64os-tasks/tools"))
from vice_harness import Monitor


def feed(mon, text):
    mon.command(0x72, bytes([len(text)]) + text)
    mon.command(0xAA)


def wait_bytes(mon, addr, expected, seconds=20):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if mon.read(addr, len(expected)) == expected:
            return
        mon.command(0xAA)
        time.sleep(.1)
    raise RuntimeError(f"Native operation did not produce expected bytes at ${addr:04x}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=6510)
    args = parser.parse_args()
    hdd = ENV / "runtime/vice support/c64os v1.09.hdd"
    original = (ENV / "baseline/os/LIBRARY/rec.lib.o.P00").read_bytes()[26:]
    assert original[:2] == b"\x00\x64" and len(original) == 217
    code = original[2:]
    # Installed 1.09 rec.lib.o: hardware presence test branches to $646D.
    # The experimental runtime is fixed at 16MB. Publish only banks 0..31,
    # without probing (and temporarily overwriting) the ReadyOS bank range.
    assert code[0x69:0x70] == bytes.fromhex("8c01df60203264")
    patched = bytearray(code)
    patched[0x6D:0x73] = bytes.fromhex("a9208d810260")
    with socket.create_connection(("127.0.0.1", args.port), 5) as sock:
        sock.settimeout(10)
        mon = Monitor(sock)
        # Verify the actual mounted image rather than trusting a port number.
        name = b"IDE64Image1"
        resource = mon.command(0x51, bytes([len(name)]) + name)
        assert resource[0] == 0
        mounted = Path(resource[2:2 + resource[1]].rstrip(b"\0").decode()).resolve()
        assert mounted == hdd.resolve(), f"Not the owned experiment HDD: {mounted}"
        feed(mon, b'LOAD"1//OS/LIBRARY/:REC.LIB.O",12,1\r')
        wait_bytes(mon, 0x6400, code)
        # Let LOAD finish before injecting the SAVE routine or another command.
        mon.command(0xAA)
        time.sleep(.5)
        mon.write(0x6400, patched)
        filename = b"@1//OS/LIBRARY/:REC.LIB.O"
        # Native SAVE from $6400 to $64D7; return normally from BASIC SYS.
        # $C0F0 holds 1 on success, 2 on KERNAL error. Preserve the error byte.
        routine = bytes([
            0xA9,len(filename),0xA2,0x80,0xA0,0xC0,0x20,0xBD,0xFF,
            0xA9,1,0xA2,12,0xA0,1,0x20,0xBA,0xFF,
            0xA9,0,0x85,0xFB,0xA9,0x64,0x85,0xFC,
            0xA9,0xFB,0xA2,0xD7,0xA0,0x64,0x20,0xD8,0xFF,
            0x8D,0xF1,0xC0,0xA9,1,0x69,0,0x8D,0xF0,0xC0,0x60])
        mon.write(0xC000, routine)
        mon.write(0xC080, filename)
        mon.write(0xC0F0, b"\0\0")
        feed(mon, b"SYS49152\r")
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            status = mon.read(0xC0F0, 2)
            if status[0]:
                assert status[0] == 1, f"Native SAVE failed: {status.hex()}"
                break
            mon.command(0xAA)
            time.sleep(.1)
        else:
            raise RuntimeError("Native SAVE did not return")
        mon.command(0xAA)
        time.sleep(.5)
        mon.write(0x6400, bytes(len(code)))
        feed(mon, b'LOAD"1//OS/LIBRARY/:REC.LIB.O",12,1\r')
        wait_bytes(mon, 0x6400, patched)
        mon.command(0xAA)
    report = {"hdd": str(hdd), "native_readback_verified": True,
              "original_sha256": hashlib.sha256(code).hexdigest(),
              "patched_sha256": hashlib.sha256(patched).hexdigest(),
              "banks": 32, "required_runtime_reu_kb": 16384,
              "note": "Capacity override for this experiment only; no generic size detection"}
    (ENV / "reu-cap-install.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
