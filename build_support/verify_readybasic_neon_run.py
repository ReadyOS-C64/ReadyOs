#!/usr/bin/env python3
"""Audit one exact ReadyOS/VICE run; safe when other probes finish concurrently."""
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[1]

def verify(manifest):
    manifest=Path(manifest)
    run=json.loads(manifest.read_text(encoding="utf-8-sig"))
    assert run["status"]=="success",run.get("failure_message")
    compiled=json.loads(Path(run["compiled_plan"]).read_text(encoding="utf-8-sig"))["Plan"]
    bitmap_paths={}
    for step in compiled["steps"]:
        if step["id"].endswith("_bitmap"):
            command=step["params"]["command"]
            bitmap_paths[step["id"][:-7]]=Path(re.search(r'bsave "([^"]+)"',command)[1])
    stages=manifest.parent/"stages"
    koa=(ROOT/"assets/readybasic/neon/rb.neon.koa").read_bytes()
    assert bitmap_paths["original"].read_bytes()==koa[2:8002],"Koala bitmap load mismatch"
    assert bitmap_paths["restored"].read_bytes()==koa[2:8002],"REU restoration mismatch"
    assert bitmap_paths["rejected"].read_bytes()==koa[2:8002],"invalid line changed bitmap"
    assert bitmap_paths["drawn"].read_bytes()!=koa[2:8002],"lines did not draw"
    expected=bytearray(koa[2:8002])
    for x0,y0,x1,y1,ink in [(0,0,159,199,1),(159,0,0,199,2),(80,0,80,199,3)]:
        dx,dy=abs(x1-x0),abs(y1-y0)
        n=max(dx,dy)
        for i in range(n+1):
            x=x0+(1 if x1>=x0 else -1)*((n//2+i*dx)//n if n else 0)
            y=y0+(1 if y1>=y0 else -1)*((n//2+i*dy)//n if n else 0)
            address=(y//8)*320+(x//4)*8+(y&7)
            shift=6-2*(x&3)
            expected[address]=(expected[address]&~(3<<shift))|(ink<<shift)
    assert bitmap_paths["drawn"].read_bytes()==expected,"MCLINE rasterization mismatch"
    def data(stage,name): return (stages/(stage+"_state")/(name+".bin")).read_bytes()
    for stage in ("picture","lines","show_a","show_b","show_c"):
        assert data(stage,"screen")==koa[8002:9002],f"{stage}: cell palette changed"
        assert bytes(v&15 for v in data(stage,"color"))==koa[9002:10002],f"{stage}: color RAM changed"
    sprite=(ROOT/"assets/readybasic/neon/rb.ready.rbr").read_bytes()[8:]
    for stage in ("show_a","show_b","show_c"):
        assert data(stage,"sprites")==sprite,"sprite data corrupted"
        assert data(stage,"pointers")==bytes(range(0x28,0x2d)),"sprite pointers lost"
        assert data(stage,"sid")==b"\x02","music stopped"
        vic=data(stage,"vic")
        assert vic[0x15]&31==31 and vic[0x1c]&31==31,"multicolor sprites not enabled"
    assert data("show_a","vic")[:10]!=data("show_b","vic")[:10],"letters did not move"
    assert data("show_a","ticks")!=data("show_b","ticks"),"music IRQ stalled"
    text=(stages/"memory_returned/decoded.txt").read_text(encoding="utf-8-sig")
    refresh=re.search(r"IMAGE REFRESHES:\s*(\d+)",text)
    assert refresh and int(refresh[1])>0,"automatic refresh never happened"
    print("NEON VERIFIED: exact Koala load/REU restore; palette-preserving lines; moving multicolor sprites; live SID; timed refresh; cleanup.")
    print(manifest)

if __name__=="__main__":
    verify(sys.argv[1])

