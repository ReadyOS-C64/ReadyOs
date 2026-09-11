#!/usr/bin/env python3
"""Visible ReadyOS/VICE proof. Build via run.sh first; never launch app directly."""
import json
import os
from pathlib import Path
import subprocess
import time
import wave
from array import array

ROOT = Path(__file__).resolve().parents[1]
audio_path = ROOT / f"build/readybasic-media-tools/media-demo-{time.time_ns()}.wav"
steps = []
def step(kind, name, **params):
    steps.append(dict(id=name, type=kind, params=params))
def keys(name, text, delay=1):
    step("input.sequence", name, keys=list(text.encode("ascii")),
         inter_key_delay_s=0.03, post_delay_s=delay)
def screen(name, text):
    step("screen.wait_contains", name, text=text, wait_timeout_s=180,
         capture_on_success=True, capture_label=name)
def mem(name, address, hexbytes):
    step("assert.memory", name, start=address,
         end=address+len(hexbytes.split())-1, equals_hex=hexbytes)

release = ROOT / "Releases/0.5/precog-d81"
disk = max(release.glob("*.d81"), key=lambda p:p.stat().st_mtime)
preboot = max(release.glob("*-preboot.prg"), key=lambda p:p.stat().st_mtime)
step("vice.launch", "boot_readyos", kill_stale=False)
screen("stock_prompt", "READY.")
keys("load_preboot_from_disk", 'LOAD "*",8\r', 3)
keys("run_preboot", 'RUN\r', 1)
screen("cold_free", "FREE:")
mem("original_ceiling", 0x37, "00 A0")
keys("ordinary_or", 'A=1:B=2:I%=0:REPEAT:I%=I%+1:UNTIL I%=3\rPRINT "ROM OR";A OR B;I%\r')
screen("ordinary_or_result", "ROM OR 3  3")
keys("ordinary_names", 'BO=7:BO=BO+1:PRINT "ORDINARY VAR";BO\r')
screen("ordinary_names_result", "ORDINARY VAR 8")
keys("border_direct", 'BORDER(2):PRINT "BORDER DIRECT";(PEEK(53280) AND 15)\r')
screen("border_direct_result", "BORDER DIRECT 2")
keys("border_mask", 'BORDER(31):PRINT "BORDER MASK";(PEEK(53280) AND 15)\r')
screen("border_mask_result", "BORDER MASK 15")
keys("border_or_argument", 'BORDER(1 OR 2):PRINT "BORDER OR";(PEEK(53280) AND 15)\r')
screen("border_or_argument_result", "BORDER OR 3")
keys("border_rom_argument", 'BORDER(ABS(-4)):PRINT "BORDER ABS";(PEEK(53280) AND 15)\r')
screen("border_rom_argument_result", "BORDER ABS 4")
keys("border_restore", 'BORDER(6)\r')
step("monitor.command", "realtime_audio", command="warp off")
step("monitor.command", "sid_8580", command='raw: resourceset "SidModel" "1"')
step("monitor.command", "audio_path", command=f'raw: resourceset "SoundRecordDeviceArg" "{audio_path}"')
step("monitor.command", "audio_start", command='raw: resourceset "SoundRecordDeviceName" "wav"')
keys("load_demo", 'LOAD "RBSND07",8\r', 3)
keys("run_demo", 'RUN\r', 0.2)
step("screen.capture", "demo_started", label="demo_started")
screen("playing", "PLAYING - BASIC")
mem("playing_state", 0xc1ff, "02")
mem("reserved_ceiling", 0x37, "00 90")
step("dump.snapshot", "playing_snapshot", stage_tag="media_playing")
# Completion alone does not prove animation: sample actual VIC colors while
# the media IRQ remains active, without changing the program or its clock.
for sample in range(8):
    step("screen.wait_contains", f"color_wait_{sample}", text="PLAYING - BASIC",
         pre_delay_s=0.33, wait_timeout_s=2)
    step("dump.memory_ranges", f"color_sample_{sample}", ranges=[
        dict(label="border", start=0xd020, end=0xd020),
        dict(label="playing", start=0xc1ff, end=0xc1ff),
        dict(label="ticks", start=0x9006, end=0x9007)])
    if sample in (0, 4):
        step("screen.capture", f"color_view_{sample}", label=f"color_view_{sample}")
screen("complete_demo", "RBSND07 DONE")
mem("released_state", 0xc1ff, "00")
mem("restored_ceiling", 0x37, "00 A0")
keys("border_demo_restored", 'PRINT "BORDER RESTORED";(PEEK(53280) AND 15)\r')
screen("border_demo_restore_result", "BORDER RESTORED 6")
keys("color_function", 'PRINT "COLOR FUNCTION";SHADE(0);SHADE(1);SHADE(15)\r')
screen("color_function_result", "COLOR FUNCTION 1  2  0")
step("assert.screen_not_contains", "demo_no_errors", not_contains="?")
step("monitor.command", "audio_stop", command='raw: resourceset "SoundRecordDeviceName" ""')
keys("cap_roundtrip", 'PRINT CHR$(147)\rCLR:A=0:B=0:A=FRE(0):MEMCAP(36864):B=FRE(0):PRINT "CAP DELTA";A-B\r')
screen("cap_delta", "CAP DELTA 4096")
keys("recap_with_live_string", 'S$=CHR$(65):MEMCAP(36864):PRINT "IDEMPOTENT OK"\r')
screen("idempotent", "IDEMPOTENT OK")
keys("reject_heap_move", 'MEMCAP(40960)\r')
screen("heap_error", "?RB ERROR")
mem("heap_error_atomic", 0x37, "00 90")
keys("clear_heap_release", 'CLR:MEMCAP(40960):PRINT "RELEASE OK"\r')
screen("release", "RELEASE OK")
mem("release_after_clr", 0x37, "00 A0")
keys("bad_alignment", 'MEMCAP(36865)\r')
screen("alignment_error", "?RB ERROR")
mem("alignment_atomic", 0x37, "00 A0")
keys("load_for_overlay", 'PRINT CHR$(147)\rMEMCAP(36864):MUSTUNE("RB.SUMMER"):MUSPLAY(1)\r', 2)
keys("measure_ticks", 'T=PEEK(36870)+256*PEEK(36871):FOR I=1 TO 200:NEXT\rPRINT "IRQ RUNS";(PEEK(36870)+256*PEEK(36871)<>T)\r')
screen("irq_advances", "IRQ RUNS-1")
keys("overlay_numeric", 'I%=0:REPEAT:I%=I%+1:KEYSCAN():UNTIL I%=30\r', 1)
# Use a known built-in command to evict media, then call it again to halt.
keys("evict_media", 'VOL(15):MUSHALT():PRINT "EVICT HALT OK"\r')
screen("evicted_halt", "EVICT HALT OK")
mem("halted_loaded", 0xc1ff, "01")
keys("measure_halted_ticks", 'T=PEEK(36870)+256*PEEK(36871):FOR I=1 TO 200:NEXT\rPRINT "IRQ HALTED";(PEEK(36870)+256*PEEK(36871)=T)\r')
screen("irq_stopped", "IRQ HALTED-1")
keys("reject_release_loaded", 'MEMCAP(40960)\r')
screen("loaded_error", "?RB ERROR")
mem("loaded_error_atomic", 0x37, "00 90")
keys("drop_final", 'MUSDROP():CLR:MEMCAP(40960):PRINT "MEDIA PROBE DONE"\r')
screen("finished", "MEDIA PROBE DONE")
mem("final_ceiling", 0x37, "00 A0")
keys("invalid_sid", 'PRINT CHR$(147)\rMEMCAP(36864):MUSTUNE("RB.BAD")\r')
screen("reject_rsid", "?RB ERROR")
mem("invalid_not_loaded", 0xc1ff, "00")
keys("missing_sid", 'PRINT CHR$(147)\rMUSTUNE("RB.ABSENT")\r')
screen("reject_missing", "?RB ERROR")
keys("resource_bad_header", 'PRINT CHR$(147)\rRSCFILE("RB.BAD")\r')
screen("resource_rejected", "?RB ERROR")
mem("resource_reject_ceiling", 0x37, "00 90")
keys("resource_release", 'MEMCAP(40960):PRINT "ALL MEDIA CHECKS DONE"\r')
screen("all_done", "ALL MEDIA CHECKS DONE")
keys("prepare_suspend", 'MEMCAP(36864):MUSTUNE("RB.SUMMER"):MUSPLAY(1)\r')
mem("before_suspend_playing", 0xc1ff, "02")
keys("exit_to_launcher", 'EXIT\r')
screen("launcher", "READY OS")
step("memory.write", "clear_launcher_keyboard", start=0xc6, bytes_hex="00")
step("monitor.command", "resume_selected_readybasic", command="keybuf \\x0d")
step("screen.capture", "resume_diagnostic", label="resume_diagnostic")
screen("resumed", "READY.")
mem("resumed_loaded_not_playing", 0xc1ff, "01")
mem("resumed_ceiling", 0x37, "00 90")
mem("resumed_kernal_ceiling", 0x283, "00 90")
keys("border_after_resume", 'BORDER(5):PRINT "BORDER RESUMED";(PEEK(53280) AND 15)\r')
screen("border_resume_result", "BORDER RESUMED 5")
keys("restart_after_suspend", 'MUSPLAY(1):PRINT "RESUMED MUSIC"\r')
screen("resumed_music", "RESUMED MUSIC")
mem("resumed_playing", 0xc1ff, "02")
keys("final_cleanup", 'MUSDROP():CLR:MEMCAP(40960):PRINT "LIFECYCLE DONE"\r')
screen("lifecycle_done", "LIFECYCLE DONE")
mem("lifecycle_final_cap", 0x37, "00 A0")
keys("occupied_file", 'OPEN 14,8,2,"RB.SUMMER"\rMEMCAP(36864):MUSTUNE("RB.SUMMER")\r')
screen("reject_occupied_file", "?RB ERROR")
mem("file_not_closed", 0x98, "01")
keys("close_owned_file", 'CLOSE 14:MEMCAP(40960)\r')
keys("cache_collision_setup", 'ZMODLD("RBM.SAMPLE2",M%):PRINT ZDOV2()\rMEMCAP(36864)\r')
mem("memcap_after_proof_overlay", 0x37, "00 90")
keys("cache_collision_cleanup", 'MEMCAP(40960):PRINT "MEDIA REGRESSION DONE"\r')
screen("regression_done", "MEDIA REGRESSION DONE")
if os.environ.get("READYBASIC_DEMO_ONLY") == "1":
    # Normal ReadyOS boot, then only the published music demo and its outcome.
    first = next(i for i, s in enumerate(steps) if s["id"] == "border_restore")
    last = next(i for i, s in enumerate(steps) if s["id"] == "audio_stop")
    steps = steps[:6] + steps[first:last+1]
plan = dict(version=1, kind="vice_task_plan", plan_id="readybasic_media_probe",
    run_mode="gui_vice", global_defaults=dict(monitor_host="127.0.0.1",
    monitor_port_start=6502, monitor_port_span=40,
    retry_policy=dict(max_attempts=1,backoff_ms=250,jitter=False),
    timeouts=dict(launch_s=45, step_s=240, read_s=2),
    artifact_policy=dict(capture_screen=True,capture_state=True,capture_dump=False),
    vice=dict(disk8=str(disk),disk9=str(disk),autostart_prg=str(preboot),autostart_enabled=False,
              drive8_type=1581,drive9_type=1581,true_drive=False,
              headless=False,close_vice=True,speed_percent=100)), steps=steps)
out = ROOT / "build/readybasic-media-tools/probe.yaml"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(plan, indent=2)+"\n")
if os.environ.get("READYBASIC_GENERATE_PLAN_ONLY") == "1":
    print(out)
    raise SystemExit(0)
project = Path(os.environ.get("VICE_TASKS_ROOT", str(ROOT.parent / "agenticdevharness/tools/vice_tasks_dotnet"))) / "src/ViceTasks.Binary/ViceTasks.Binary.csproj"
previous_runs = set((ROOT / "logs").glob("vice_auto_*/manifest.json"))
subprocess.run(["dotnet","run","--project",str(project),"--","run","--plan",str(out),"--close-vice"],cwd=ROOT,check=True)
new_runs = set((ROOT / "logs").glob("vice_auto_*/manifest.json")) - previous_runs
matching = [p for p in new_runs if p.parent.name != "vice_auto_latest"
            and json.loads(p.read_text(encoding="utf-8-sig")).get("plan_id") == plan["plan_id"]]
assert len(matching) == 1, "expected exactly one media probe run"
run_dir = matching[0].parent
colors, ticks = [], []
for sample in range(8):
    stage = run_dir / "stages" / f"color_sample_{sample}"
    assert (stage / "playing.bin").read_bytes() == b"\x02", "music stopped during animation"
    colors.append((stage / "border.bin").read_bytes()[0] & 15)
    ticks.append(int.from_bytes((stage / "ticks.bin").read_bytes(), "little"))
assert len(set(colors)) >= 3, f"border did not visibly cycle: {colors}"
assert len(set(ticks)) >= 3, f"music IRQ did not advance: {ticks}"
print(f"ANIMATION VERIFIED: border colors {colors}; music ticks {ticks}")
with wave.open(str(audio_path)) as recording:
    assert recording.getsampwidth() == 2, "expected 16-bit VICE WAV"
    pcm = recording.readframes(recording.getnframes())
    samples = array("h", pcm)
    peak = max(abs(value) for value in samples)
    seconds = len(samples) / recording.getnchannels() / recording.getframerate()
    assert peak > 500 and len(set(samples)) > 32, "SID recording is silent/constant"
    # The harness terminates VICE, so its WAV can retain placeholder RIFF sizes.
    # Write a separate finalized capture from actual PCM; preserve the raw file.
    final_audio = audio_path.with_name(audio_path.stem + "-final.wav")
    with wave.open(str(final_audio), "wb") as finalized:
        finalized.setparams(recording.getparams())
        finalized.writeframes(pcm)
    print(f"AUDIO VERIFIED: {seconds:.2f}s, peak={peak}; {final_audio}")
