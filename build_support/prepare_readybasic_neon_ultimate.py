#!/usr/bin/env python3
"""Build an exact-path Ultimate D81 and hardware UI plan, without contacting it.

Deploy/boot with a Terminal-owned shell, let ReadyOS finish loading, then run
the emitted plan from Terminal. The plan attaches to ReadyBASIC already booted
through the normal ReadyOS boot chain; it does not launch a standalone app.
"""
import json
import os
from pathlib import Path
import subprocess
import uuid

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"build/readybasic-media-tools"
tag=uuid.uuid4().hex[:8]
remote=f"/USB1/automation/readybasic-media/neon-{tag}/RB{tag}.D81"
catalog=OUT/f"neon-ultimate-{tag}.ini"
catalog.write_text((ROOT/"cfg/profiles/precog-ultimate.ini").read_text().replace(
    "c64u_image_path=\n",f"c64u_image_path={remote.lower()}\n"))
subprocess.run(["/bin/bash","./run.sh","--profile","precog-ultimate","--config",str(catalog),
                "--run-first","readybasic","--build-only"],cwd=ROOT,check=True)
disk=max((ROOT/"Releases/0.5/precog-ultimate").glob("*.d81"),key=lambda p:p.stat().st_mtime)
cfg=OUT/f"neon-apps-{tag}.cfg"
subprocess.run(["c1541",str(disk),"-read","apps.cfg,s",str(cfg)],check=True)
text=cfg.read_bytes().decode("ascii").lower()
assert f"c64u_image_path={remote.lower()}\r" in text and "dma_loading=1\r" in text
assert "runappfirst=readybasic\r" in text
subprocess.run(["python3","build_support/verify_release_directory_order.py","--profile","precog-ultimate"],cwd=ROOT,check=True)
steps=[]
def add(kind,name,**params):steps.append(dict(type=kind,id=name,params=params))
def screen(name,text,delay=0):
    add("screen.wait_contains",name,text=text,pre_delay_s=delay,poll_s=1,wait_timeout_s=180,capture_label=name)
def keys(name,text,delay=1):
    add("input.sequence",name,keys=list(text.encode()),inter_key_delay_s=.05,post_delay_s=delay)
def capture(name):
    add("assert.screen_not_contains",name+"_no_error",not_contains="?")
    add("screen.capture",name,label=name,capture_state=True)
    add("dump.memory_ranges",name+"_state",ranges=[
        dict(label="screen",start=0xcc00,end=0xcfe7),
        dict(label="sprites",start=0xca00,end=0xcb3f),
        dict(label="ticks",start=0x9006,end=0x9007),
        dict(label="sid",start=0xc1ff,end=0xc1ff)])
add("ultimate.launch","attach_readyos",boot_mode="none",drives=[
    dict(slot="a",bus_id=8,drive_type="1581",enabled=True,disk="",remote_disk="",mount_mode="unlinked")])
screen("readybasic","READYBASIC")
add("ultimate.speed.set","normal_speed",mhz=1)
keys("load_demo",'LOAD "RBSND08",8\r',20)
keys("run_demo",'RUN\r',90)  # No REST polling while SEQ files load over IEC.
screen("show_running","ORBITAL SHOW RUNNING")
for speed in (1,16,64):
    add("ultimate.speed.set",f"speed_{speed}",mhz=speed)
    screen(f"settle_{speed}","ORBITAL SHOW RUNNING",3)
    capture(f"show_{speed}_a")
    screen(f"motion_{speed}","ORBITAL SHOW RUNNING",2)
    capture(f"show_{speed}_b")
add("ultimate.speed.set","return_1mhz",mhz=1)
keys("quit",'Q',2)
screen("clean_exit","BASIC MEMORY RETURNED:")
add("assert.memory","released_sid",start=0xc1ff,end=0xc1ff,equals_hex="00")
add("assert.memory","released_memory",start=0x37,end=0x38,equals_hex="00 A0")
add("assert.screen_not_contains","no_error",not_contains="?")
plan=dict(version=1,kind="ultimate_task_plan",plan_id="readybasic_neon_ultimate_"+tag,
    run_mode="ultimate64",steps=steps,global_defaults=dict(
    retry_policy=dict(max_attempts=1,backoff_ms=500,jitter=False),
    timeouts=dict(launch_s=90,step_s=360,read_s=20),
    artifact_policy=dict(capture_screen=True,capture_state=True,capture_dump=False),
    ultimate=dict(host=os.environ.get("C64U_HOST","10.0.0.79"),
      remote_root=remote.rsplit("/",1)[0].lstrip("/"),disk8="",disk9="",autostart_prg="",
      drive_a_bus_id=8,drive_a_type="1581",drive_a_enabled=True,drive_b_enabled=False,
      mount_mode="unlinked",boot_drive=8,capture_video_stream=True,stream_port=11002,
      stream_timeout_s=4,default_speed_mhz=1,warp_equivalent_mhz=16)))
path=OUT/f"neon-ultimate-{tag}.yaml"
path.write_text(json.dumps(plan,indent=2)+"\n")
(OUT/"neon-deployment.json").write_text(json.dumps(dict(disk=str(disk),remote=remote,plan=str(path)),indent=2)+"\n")
print("PREPARED:",disk,remote,path)
