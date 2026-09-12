#!/usr/bin/env python3
"""Offline Warped City -> C64 Koala conversion, not part of normal builds.

Requires Pillow and numpy. Uses the same palette/error metric and reserved
unused drawing colors as build_support/prepare_readybasic_neon.py.
Only CC0 artwork is used; no music, characters, or demo code from the archive.
"""
from pathlib import Path
from itertools import combinations
import hashlib
import json
import numpy as np
from PIL import Image, ImageEnhance

OUT = Path(__file__).resolve().parent
PALETTE = np.array([
    (0,0,0),(255,255,255),(136,57,50),(103,182,189),
    (139,63,150),(85,160,73),(64,49,141),(191,206,114),
    (139,84,41),(87,66,0),(184,105,98),(80,80,80),
    (120,120,120),(148,224,137),(120,105,196),(159,159,159)], dtype=np.int32)


def source_picture():
    # Fit the full skyline and foreground width into a 320x200 display area.
    # Leave the upper third mostly sky for the existing READY sprite wave.
    sky = Image.open(OUT / "source/skyline-a.png").convert("RGBA")
    canvas = Image.new("RGBA", (320,200))
    for x in range(0,320,sky.width):
        canvas.alpha_composite(sky, (x,0))
    # Reuse one round planet from the alternate sky instead of stretching its
    # narrow 128x240 layer across the screen (which would flatten the planet).
    alternate = Image.open(OUT / "source/skyline-b.png").convert("RGBA")
    canvas.alpha_composite(alternate.crop((59,37,107,84)), (172,22))
    towers = Image.open(OUT / "source/near-buildings-bg.png").convert("RGBA")
    towers = towers.resize((320,136), Image.Resampling.NEAREST)
    canvas.alpha_composite(towers, (0,64))
    source = ImageEnhance.Brightness(canvas.convert("RGB")).enhance(2.4)
    source.save(OUT / "composition-preview.png")
    return source.resize((160,200), Image.Resampling.BOX)


def encode(source):
    pixels = np.array(source, dtype=np.int32)
    triples = np.array(list(combinations(range(1,16),3)))
    bitmap, screen, color = bytearray(8000), bytearray(), bytearray()
    for cy in range(25):
        for cx in range(40):
            cell = pixels[cy*8:cy*8+8,cx*4:cx*4+4].reshape(32,3)
            dist = ((cell[:,None,:]-PALETTE[None,:,:])**2 * [2,3,2]).sum(axis=2)
            cost = np.minimum(dist[:,0,None], dist[:,triples].min(axis=2)).sum(axis=0)
            chosen = triples[int(cost.argmin())].tolist()
            used = set(np.argmin(dist[:,[0]+chosen],axis=1).tolist())
            for slot in range(1,4):
                if slot not in used:
                    chosen[slot-1] = next(c for c in (3,14,7,1,10,13) if c not in chosen)
            codes = dist[:,[0]+chosen].argmin(axis=1).reshape(8,4)
            for row in range(8):
                bitmap[(cy*40+cx)*8+row] = sum(int(codes[row,x]) << (6-2*x) for x in range(4))
            screen.append(chosen[0]*16+chosen[1])
            color.append(chosen[2])
    return b"\x00\x60" + bitmap + screen + color + b"\x00"


def verify_and_preview(data):
    # Decode the written resource, not the intermediate source image, so the
    # preview includes exactly the pixel/code and per-cell color constraints.
    assert len(data) == 10003 and data[:2] == b"\x00\x60"
    assert data[-1] == 0 and max(data[9002:10002]) < 16
    decoded = np.zeros((200,160,3), dtype=np.uint8)
    seen = set()
    for cy in range(25):
        for cx in range(40):
            cell = cy*40+cx
            screen = data[8002+cell]
            pal = [data[-1], screen >> 4, screen & 15, data[9002+cell]]
            assert len(set(pal)) == 4
            for row in range(8):
                byte = data[2+cell*8+row]
                for x in range(4):
                    index = pal[(byte >> (6-2*x)) & 3]
                    seen.add(index)
                    decoded[cy*8+row,cx*4+x] = PALETTE[index]
    Image.fromarray(decoded).resize((640,400),Image.Resampling.NEAREST).save(OUT / "background-preview.png")
    return {"bytes": len(data), "logical_pixels": [160,200], "cells": [40,25],
            "global_background": data[-1], "visible_palette_indices": sorted(seen),
            "all_cells_have_four_distinct_palette_slots": True}


if __name__ == "__main__":
    data = encode(source_picture())
    (OUT / "rb.warp.koa").write_bytes(data)
    result = verify_and_preview((OUT / "rb.warp.koa").read_bytes())
    # Preserve original-download provenance without shipping the full archive,
    # which also contains unused music governed by separate attribution terms.
    result["original_archive"] = {
        "url": "https://opengameart.org/sites/default/files/warped_city_files.zip",
        "sha256": "cf0e69a203206f529adbaf1f82d4c5f165ca9cdb49d3995ec88d135b37e40e3e",
        "local_provenance_copy": "build/readybasic-media-tools/warped-city-source.zip",
        "required_to_regenerate": False,
    }
    names = ["rb.warp.koa", "background-preview.png"]
    names += [str(p.relative_to(OUT)) for p in sorted((OUT / "source").glob("*"))]
    result["sha256"] = {n: hashlib.sha256((OUT/n).read_bytes()).hexdigest() for n in names}
    (OUT / "checksums.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({k:v for k,v in result.items() if k != "sha256"}))
