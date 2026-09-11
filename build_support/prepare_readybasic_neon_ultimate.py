#!/usr/bin/env python3
"""Build an exact-path Ultimate D81 and hardware UI plan, without contacting it.

Deploy/test with run_readybasic_gfxsnd_ultimate.py from a Terminal-owned shell.
The emitted plan is attach-only configuration, not a standalone demo test.
Demo loading must use the video gate: fixed waits followed by REST screen/RAM
reads can interrupt IEC loading and lock up physical C64U firmware.
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
add("ultimate.launch","attach_readyos",boot_mode="none",drives=[
    dict(slot="a",bus_id=8,drive_type="1581",enabled=True,disk="",remote_disk="",mount_mode="unlinked")])
screen("readybasic","READYBASIC")
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
