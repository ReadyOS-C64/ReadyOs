#!/usr/bin/env python3
"""Compile CC0 art and original pixel-grid glyphs to C64 resources (Pillow/numpy).

Offline asset preparation only; normal ReadyOS builds use the checked-in files.
Koala output obeys one global background plus three colors per 4x8 logical cell.
"""
from pathlib import Path
from zipfile import ZipFile
from itertools import combinations
import hashlib
import json
import struct
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets/readybasic/neon"
PALETTE = np.array([
    (0,0,0),(255,255,255),(136,57,50),(103,182,189),
    (139,63,150),(85,160,73),(64,49,141),(191,206,114),
    (139,84,41),(87,66,0),(184,105,98),(80,80,80),
    (120,120,120),(148,224,137),(120,105,196),(159,159,159)], dtype=np.int32)

def picture():
    with ZipFile(OUT / "space_background_pack.zip") as z:
        def layer(name):
            return Image.open(z.open("space_background_pack/layers/parallax-space-"+name+".png")).convert("RGBA")
        canvas = layer("backgound")
        for name, xy in [("stars",(0,0)),("far-planets",(0,0)),
                         ("big-planet",(151,63)),("ring-planet",(29,43))]:
            canvas.alpha_composite(layer(name),xy)
    # Layer composition omits the PSD's preview labels. A modest exposure boost
    # keeps the artist's dark red/purple scene legible on the C64 palette.
    source = ImageEnhance.Brightness(canvas.convert("RGB")).enhance(1.65)
    source = source.resize((160,200),Image.Resampling.LANCZOS)
    pixels = np.array(source,dtype=np.int32)
    triples = np.array(list(combinations(range(1,16),3)))
    bitmap, screen, color = bytearray(8000),bytearray(),bytearray()
    decoded = np.zeros((200,160,3),dtype=np.uint8)
    for cy in range(25):
        for cx in range(40):
            cell = pixels[cy*8:cy*8+8,cx*4:cx*4+4].reshape(32,3)
            dist = ((cell[:,None,:]-PALETTE[None,:,:])**2 * [2,3,2]).sum(axis=2)
            cost = np.minimum(dist[:,0,None],dist[:,triples].min(axis=2)).sum(axis=0)
            chosen = triples[int(cost.argmin())].tolist()
            # Keep unused palette slots visibly distinct for additive line art.
            used = set(np.argmin(dist[:,[0]+chosen],axis=1).tolist())
            for slot in range(1,4):
                if slot not in used:
                    chosen[slot-1] = next(c for c in (3,14,7,1,10,13) if c not in chosen)
            pal = [0]+chosen
            codes = dist[:,pal].argmin(axis=1).reshape(8,4)
            offset = (cy*40+cx)*8
            for row in range(8):
                bitmap[offset+row] = sum(int(codes[row,x]) << (6-2*x) for x in range(4))
            screen.append(chosen[0]*16+chosen[1]); color.append(chosen[2])
            decoded[cy*8:cy*8+8,cx*4:cx*4+4] = PALETTE[np.array(pal)[codes]]
    (OUT / "rb.neon.koa").write_bytes(b"\x00\x60"+bitmap+screen+color+b"\x00")
    Image.fromarray(decoded).resize((640,400),Image.Resampling.NEAREST).save(OUT / "background-preview.png")

def sprites():
    # Original chamfered, heavy display lettering, 12 multicolor pixels x 21.
    outlines = {
      "R": ([(1,1),(8,1),(10,3),(10,8),(8,10),(11,18),(7,18),(4,12),(3,12),(3,18),(1,18)], [(4,4),(7,4),(7,7),(4,7)]),
      "E": ([(1,1),(10,1),(10,4),(4,4),(4,8),(9,8),(9,11),(4,11),(4,15),(10,15),(10,18),(1,18)], []),
      "A": ([(0,18),(0,6),(4,1),(7,1),(10,6),(10,18),(7,18),(7,12),(3,12),(3,18)], [(3,6),(5,3),(7,6),(7,9),(3,9)]),
      "D": ([(1,1),(7,1),(10,4),(10,15),(7,18),(1,18)], [(4,4),(6,4),(7,5),(7,14),(6,15),(4,15)]),
      "Y": ([(0,1),(3,1),(5,7),(7,1),(10,1),(7,11),(7,18),(4,18),(4,11)], []),
    }
    payload = bytearray()
    preview = Image.new("RGB",(5*56,42),(15,10,30))
    for index, (letter,(outer,hole)) in enumerate(outlines.items()):
        mask=Image.new("1",(12,21)); draw=ImageDraw.Draw(mask)
        draw.polygon(outer,fill=1)
        if hole: draw.polygon(hole,fill=0)
        shape=np.array(mask)
        codes=np.zeros((21,12),dtype=np.uint8)
        # One-pixel blue extrusion; bright edge and split chrome face.
        for y in range(21):
            for x in range(12):
                if y and x and shape[y-1,x-1]: codes[y,x]=3
                if shape[y,x]:
                    codes[y,x]=1 if y in (1,2,11) or not shape[max(0,y-1),x] else (3 if y==10 else 2)
        for y in range(21):
            for x in range(0,12,4):
                payload.append(sum(int(codes[y,x+j]) << (6-2*j) for j in range(4)))
        payload.append(0)
        rgb=PALETTE[np.array([0,1,3,6])[codes]].astype(np.uint8)
        rgba=Image.fromarray(rgb).convert("RGBA")
        rgba.putalpha(Image.fromarray((codes!=0).astype(np.uint8)*255))
        rgba=rgba.resize((48,42),Image.Resampling.NEAREST)
        preview.paste(rgba,(index*56,0),rgba)
    assert len(payload)==320
    (OUT / "rb.ready.rbr").write_bytes(b"RBR1"+struct.pack("<HH",0xca00,len(payload))+payload)
    preview.resize((840,126),Image.Resampling.NEAREST).save(OUT / "sprites-preview.png")

if __name__ == "__main__":
    picture(); sprites()
    names=["space_background_pack.zip","rb.neon.koa","rb.ready.rbr"]
    (OUT / "checksums.json").write_text(json.dumps({n:hashlib.sha256((OUT/n).read_bytes()).hexdigest() for n in names},indent=2)+"\n")
    print("Compiled Koala (10003 bytes) and five sprites (328-byte RBR1).")
