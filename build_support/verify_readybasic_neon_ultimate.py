#!/usr/bin/env python3
"""Audit a completed physical-Ultimate RBSND08 UI run, including real frames."""
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
run = Path(sys.argv[1])
manifest = json.loads((run / "manifest.json").read_text(encoding="utf-8-sig"))
assert manifest["status"] == "success", manifest.get("failure_message")
assert manifest["backend"] == "ultimate64"
assert manifest["plan_id"].startswith("readybasic_neon_ultimate_")
stages = run / "stages"
koa = (ROOT / "assets/readybasic/neon/rb.neon.koa").read_bytes()
sprites = (ROOT / "assets/readybasic/neon/rb.ready.rbr").read_bytes()[8:]
for speed in (1, 16, 64):
    frames, ticks = [], []
    for sample in ("a", "b"):
        stage = f"show_{speed}_{sample}"
        state = stages / (stage + "_state")
        assert (state / "screen.bin").read_bytes() == koa[8002:9002], stage
        color=state / "color.bin"
        if color.exists():
            assert bytes(v&15 for v in color.read_bytes()) == koa[9002:10002], stage
        assert (state / "sprites.bin").read_bytes() == sprites, stage
        assert (state / "sid.bin").read_bytes() == b"\x02", stage
        ticks.append((state / "ticks.bin").read_bytes())
        frame = stages / stage / "screen.png"
        assert frame.is_file(), f"{stage}: no actual video frame captured"
        frames.append(frame.read_bytes())
    assert ticks[0] != ticks[1], f"{speed} MHz: music IRQ stalled"
    assert frames[0] != frames[1], f"{speed} MHz: picture did not animate"
    print(f"{speed} MHz: live video changed, SID advanced, sprite/palette data intact")
text = (stages / "clean_exit/decoded.txt").read_text(encoding="utf-8-sig")
refresh = re.search(r"IMAGE REFRESHES:\s*(\d+)", text)
assert refresh and int(refresh[1]) > 0, "no automatic image refresh recorded"
print("ULTIMATE NEON VERIFIED: 1/16/64 MHz, real video, music, refresh and clean exit")
