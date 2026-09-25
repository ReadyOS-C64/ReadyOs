#!/usr/bin/env python3
"""Generate a physical-Ultimate media proof. Execute from Terminal-owned bash.

Builds through run.sh with apps.cfg pointing to the exact fresh upload path.
This never starts or closes VICE and never boots an app directly.
"""
import json
import os
from pathlib import Path
import subprocess
import uuid

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/readybasic-media-tools"
tag = uuid.uuid4().hex[:8]
remote = f"USB1/automation/readybasic-media/{tag}/RBM{tag}.D81"
OUT.mkdir(parents=True, exist_ok=True)
catalog = OUT / f"ultimate-{tag}.ini"
catalog.write_text((ROOT / "cfg/profiles/precog-ultimate.ini").read_text().replace(
    "c64u_image_path=", "c64u_image_path=/" + remote.lower()))
subprocess.run(["/bin/bash", "./run.sh", "--profile", "precog-ultimate",
                "--config", str(catalog), "--run-first", "readybasic", "--build-only"],
               cwd=ROOT, check=True)
subprocess.run(["python3", str(ROOT / "build_support/run_readybasic_media_probe.py")],
               env={**os.environ, "READYBASIC_GENERATE_PLAN_ONLY": "1"}, check=True)
plan = json.loads((OUT / "probe.yaml").read_text())
release = ROOT / "Releases/0.5/precog-ultimate"
manifest = json.loads((release / "manifest.json").read_text())
disk = next(Path(d['path']) for d in manifest['disks'] if d['drive'] == 8)
examples = next(Path(d['path']) for d in manifest['disks'] if d['drive'] == 9)
remote_examples = remote.rsplit('/', 1)[0] + '/EXAMPLES.D81'
steps = [dict(id="boot_ultimate_readyos", type="ultimate.launch", params=dict(
    boot_mode="disk", reset_before_boot=True, post_reset_delay_s=3,
    boot_command='LOAD "*",8,1\nRUN\n', boot_drive=8,
    drives=[dict(slot="a", bus_id=8, drive_type="1581", enabled=True,
                 disk=str(disk), remote_disk=remote, mount_mode="unlinked"),
            dict(slot="b", bus_id=9, drive_type="1581", enabled=True,
                 disk=str(examples), remote_disk=remote_examples, mount_mode="unlinked")]))]
skip = {"boot_readyos", "stock_prompt", "load_preboot_from_disk", "run_preboot",
        "clear_launcher_keyboard", "playing_state", "reserved_ceiling"}
for item in plan["steps"]:
    if item["id"] in skip or item["type"] == "dump.snapshot":
        continue
    if item["type"] == "monitor.command":
        if item["id"] == "resume_selected_readybasic":
            item = dict(id=item["id"], type="input.sequence", params=dict(
                keys=[13], inter_key_delay_s=0.05, post_delay_s=2))
        else:
            continue
    if item["type"] == "screen.wait_contains":
        item["params"]["wait_timeout_s"] = 240
        item["params"]["poll_s"] = 1
        if item["id"] == "cold_free":
            # Existing hardware probes document DMA screen polling stalling
            # KERNAL IEC LOAD. Let the complete ReadyOS boot run quietly first.
            item["params"].update(pre_delay_s=180, wait_timeout_s=360)
    if item["type"] == "input.sequence":
        item["params"]["keys"] = list(bytes(item["params"]["keys"]).replace(b'LOAD "RBSND07",8', b'LOAD "RBSND07",9'))
        typed = bytes(item["params"]["keys"]).decode("ascii")
        if any(word in typed for word in ("LOAD ", "MUSTUNE", "RSCFILE", "LDMOD")):
            item["params"]["post_delay_s"] = 20
        if item["id"] == "run_demo":
            item["params"]["post_delay_s"] = 30
    steps.append(item)

def add(kind, name, **params):
    steps.append(dict(id=name, type=kind, params=params))
def keys(name, text):
    add("input.sequence", name, keys=list(text.encode("ascii")),
        inter_key_delay_s=0.03, post_delay_s=20 if "MUSTUNE" in text else 1)
def screen(name, text):
    add("screen.wait_contains", name, text=text, wait_timeout_s=120,
        capture_on_success=True, capture_label=name)
def mem(name, address, value):
    add("assert.memory", name, start=address, end=address, equals_hex=value)

for speed in (16, 64):
    label = f"turbo_{speed}"
    add("ultimate.speed.set", label, mhz=speed)
    keys(label + "_load", 'PRINT CHR$(147)\rMEMCAP(36864):MUSTUNE("RB.SUMMER"):MUSPLAY(1)\r')
    mem(label + "_playing", 0xc1ff, "02")
    keys(label + "_tick", 'T=PEEK(36870)+256*PEEK(36871):FOR I=1 TO 1000:NEXT\r'
         f'PRINT "TICK {speed}";(PEEK(36870)+256*PEEK(36871)<>T)\r')
    screen(label + "_advances", f"TICK {speed}-1")
    keys(label + "_evict", f'KEYSCAN():VOL(15):MUSHALT():PRINT "HALT {speed}"\r')
    mem(label + "_halted", 0xc1ff, "01")
    keys(label + "_stop_tick", 'T=PEEK(36870)+256*PEEK(36871):FOR I=1 TO 1000:NEXT\r'
         f'PRINT "STOP {speed}";(PEEK(36870)+256*PEEK(36871)=T)\r')
    screen(label + "_stopped", f"STOP {speed}-1")
    keys(label + "_drop", f'MUSDROP():CLR:MEMCAP(40960):PRINT "DONE {speed}"\r')
    mem(label + "_empty", 0xc1ff, "00")
    screen(label + "_complete", f"DONE {speed}")
add("ultimate.speed.set", "return_normal_speed", mhz=1)
keys("physical_done", 'PRINT CHR$(147);"READYBASIC MEDIA HARDWARE PASS"\r')
screen("hardware_complete", "READYBASIC MEDIA HARDWARE PASS")
add("state.capture", "hardware_final_state", label="hardware_final")
plan.update(kind="ultimate_task_plan", plan_id="readybasic_media_ultimate_" + tag,
            run_mode="ultimate64", steps=steps)
plan["global_defaults"] = dict(
    retry_policy=dict(max_attempts=1, backoff_ms=500, jitter=False),
    timeouts=dict(launch_s=90, step_s=660, read_s=20),
    artifact_policy=dict(capture_screen=True, capture_state=True, capture_dump=False),
    ultimate=dict(host=os.environ.get("C64U_HOST", "10.0.0.79"),
                  remote_root=f"USB1/automation/readybasic-media/{tag}",
                  disk8=str(disk), disk9="", autostart_prg="",
                  drive_a_bus_id=8, drive_a_type="1581", drive_a_enabled=True,
                  drive_b_enabled=False, mount_mode="unlinked", boot_drive=8,
                  capture_video_stream=True, stream_port=11000, stream_timeout_s=4,
                  default_speed_mhz=1, warp_equivalent_mhz=16))
output = OUT / f"ultimate-{tag}.yaml"
output.write_text(json.dumps(plan, indent=2) + "\n")
print(output)
print(f"Physical test image: /{remote}; {len(steps)} steps")
if os.environ.get("READYBASIC_GENERATE_PLAN_ONLY") == "1":
    raise SystemExit(0)
harness = ROOT.parent / "agenticdevharness/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
subprocess.run(["/usr/local/share/dotnet/dotnet", "run", "--project", str(harness),
                "--", "run-ultimate-plan", "--plan", str(output), "--no-tui"],
               cwd=ROOT, check=True)
