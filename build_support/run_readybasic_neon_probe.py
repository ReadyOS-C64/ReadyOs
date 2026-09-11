#!/usr/bin/env python3
"""Visible VICE proof of the published RBSND08 demo; normal ReadyOS boot only."""
import json
import os
from pathlib import Path
import re
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"build/readybasic-media-tools"
subprocess.run(["python3",str(ROOT/"build_support/verify_readybasic_neon.py")],check=True)
subprocess.run(["python3",str(ROOT/"build_support/run_readybasic_media_probe.py")],
               env={**os.environ,"READYBASIC_GENERATE_PLAN_ONLY":"1"},check=True)
plan=json.loads((OUT/"probe.yaml").read_text())
steps=plan["steps"][:6]
# Give the disk boot an uninterrupted interval; aggressive monitor polling can
# interfere with IEC loading. This is a wait bound, not C64 protocol pacing.
steps[4]["params"].update(pre_delay_s=15,poll_s=1,wait_timeout_s=240)
tag=str(time.time_ns())
def add(kind,name,**params): steps.append(dict(id=name,type=kind,params=params))
def keys(name,text,delay=1):
    add("input.sequence",name,keys=list(text.encode("ascii")),inter_key_delay_s=.03,post_delay_s=delay)
def screen(name,text,delay=0):
    add("screen.wait_contains",name,text=text,pre_delay_s=delay,wait_timeout_s=120,capture_label=name)
def mem(name,start,data):
    add("assert.memory",name,start=start,end=start+len(data)-1,equals_hex=data.hex(" "))
def capture(name):
    add("assert.screen_not_contains",name+"_no_error",not_contains="?")
    add("screen.capture",name,label=name)
    add("dump.memory_ranges",name+"_state",ranges=[
        dict(label="vic",start=0xd000,end=0xd02e),
        dict(label="screen",start=0xcc00,end=0xcfe7),
        dict(label="color",start=0xd800,end=0xdbe7),
        dict(label="sprites",start=0xca00,end=0xcb3f),
        dict(label="pointers",start=0xcff8,end=0xcffc),
        dict(label="sid",start=0xc1ff,end=0xc1ff),
        dict(label="ticks",start=0x9006,end=0x9007)])
    if name.startswith("show_"):
        add("dump.memory_ranges",name+"_basic",ranges=[
            dict(label="pointers",start=0x2d,end=0x38),
            dict(label="jiffies",start=0xa0,end=0xa2),
            dict(label="variables",start=0x3400,end=0x3fff)])
bitmap_paths={}
def bitmap(name):
    path=OUT/f"neon-{tag}-{name}.bin"
    bitmap_paths[name]=path
    add("monitor.command",name+"_ram",command="raw: bank ram")
    add("monitor.command",name+"_bitmap",command=f'raw: bsave "{path}" 0 e000 ff3f')
    add("monitor.command",name+"_cpu",command="raw: bank cpu")

keys("prelude_program",'NEW\r'
    '10 ZMODLD("RBM.MEDIA",M%):PRINT "IMAGE READY"\r'
    '20 GFXMODE("MBITMAP"):GFXCLEAR(0):MCFILE("RB.NEON")\r'
    '30 H%=GFXSURF("MBITMAP"):GFXTGT(H%):GFXSYNC():GFXTGT(0)\r'
    '40 GET A$:IF A$<>"D" THEN 40\r'
    '50 MCLINE(0,0,159,199,1):MCLINE(159,0,0,199,2)\r'
    '60 MCLINE(80,0,80,199,3)\r'
    '70 GET A$:IF A$<>"R" THEN 70\r'
    '80 GFXBLIT(H%)\r'
    '90 GET A$:IF A$<>"X" THEN 90\r'
    '100 BUFDROP(H%):END\r')
keys("load_resources",'RUN\r',8)
screen("image_ready","IMAGE READY")
bitmap("original")
capture("picture")
keys("draw_palette_slots",'D')
bitmap("drawn")
capture("lines")
keys("restore_image",'R')
bitmap("restored")
keys("end_prelude",'X')
keys("bad_line",'MCLINE(160,0,0,199,1)\r')
screen("reject_bad_line","?RB ERROR")
bitmap("rejected")
keys("cleanup_prelude",'GFXTEXT():BORDER(6):NEW\r')
keys("load_demo",'LOAD "RBSND08",8\r',3)
add("monitor.command","realtime",command="warp off")
keys("run_demo",'RUN\r',.2)
screen("demo_loaded","ORBITAL SHOW RUNNING")
steps[-1]["params"].update(pre_delay_s=45,poll_s=1,wait_timeout_s=180)
mem("music_playing",0xc1ff,b"\x02")
mem("reserved",0x37,b"\x00\x90")
capture("show_a")
bitmap("show_a")
screen("wait_motion","Q QUITS / IMAGE",2)
capture("show_b")
screen("wait_refresh","Q QUITS / IMAGE",32)
capture("show_c")
keys("quit_demo",'Q',2)
screen("demo_done","ORBITAL ECHOES COMPLETE")
screen("memory_returned","BASIC MEMORY RETURNED:")
mem("music_released",0xc1ff,b"\x00")
mem("memory_released",0x37,b"\x00\xa0")
add("assert.screen_not_contains","no_demo_error",not_contains="?")
plan.update(plan_id="readybasic_neon_probe",steps=steps)
path=OUT/"neon-probe.yaml"
path.write_text(json.dumps(plan,indent=2)+"\n")
if os.environ.get("READYBASIC_GENERATE_PLAN_ONLY")=="1":
    print(path);raise SystemExit(0)
old=set((ROOT/"logs").glob("vice_auto_*/manifest.json"))
project=ROOT.parent/"agenticdevharness/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
subprocess.run(["dotnet","run","--project",str(project),"--","run","--plan",str(path),"--close-vice"],cwd=ROOT,check=True)
runs={p for p in (ROOT/"logs").glob("vice_auto_*/manifest.json")
      if p.parent.name!="vice_auto_latest"}-old
manifest=next(p for p in runs if json.loads(p.read_text(encoding="utf-8-sig"))["plan_id"]==plan["plan_id"])
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
