#!/usr/bin/env python3
"""Generate launcher return acceptance plans for the shared .NET UI framework.

This file only prepares fixtures/plans. All device input, capture, assertions,
speed control, and result reporting belong to vice_tasks_dotnet.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", choices=("d81", "easyflash", "ultimate"), required=True)
    parser.add_argument("--disk", type=Path, required=True)
    parser.add_argument("--boot", type=Path, help="D81 PREBOOT PRG or EasyFlash CRT")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    ultimate = args.target == "ultimate"
    cartridge = args.target == "easyflash"
    args.out.parent.mkdir(parents=True, exist_ok=True)
    defaults = dict(retry_policy=dict(max_attempts=1),
        timeouts=dict(launch_s=120, step_s=180, read_s=30),
        artifact_policy=dict(capture_screen=True, capture_state=True, capture_dump=False))
    plan = dict(version=1, kind="ultimate_task_plan" if ultimate else "vice_task_plan",
        plan_id="launcher_return_" + args.target,
        run_mode="ultimate64" if ultimate else "gui_vice", global_defaults=defaults, steps=[])

    def step(name, kind, **params):
        plan["steps"].append(dict(id=name, type=kind, params=params))

    def wait(name, text, delay=0):
        step(name, "screen.wait_contains", text=text, pre_delay_s=delay,
             wait_timeout_s=180, poll_s=0.5, capture_label=name)

    def keys(name, values, delay=0.5):
        if ultimate:
            step(name, "input.sequence", keys=values, inter_key_delay_s=0.2,
                 post_delay_s=delay)
        else:
            for i, value in enumerate(values):
                # Use the framework's supported text-monitor fallback to keep
                # the keyboard byte/count writes atomic. Its standard binary
                # input.key path stalls this resume test on both old/new builds.
                # The framework still owns transport, resume, capture and checks.
                step(f"{name}_{i}", "monitor.command", command=
                     f"raw:> 00c5 40\n> 0277 {value:02x}\n> 00c6 01")

    footer = "F2:NEXT APP  F4:PREV  STOP:QUIT"
    if ultimate:
        tag = time.strftime("FRAMEWORK-RETURN-%Y%m%d-%H%M%S") + f"-{os.getpid()}"
        remote = "USB1/READYOS_SETUP_TEST/" + tag
        disk = args.out.parent / (tag + ".d81")
        subprocess.run(["python3", str(ROOT / "build_support/prepare_setup_fixture.py"),
            "--source", str(args.disk.resolve()), "--output", str(disk.resolve()),
            "--saved-path", "/" + remote + "/RETURN.D81"], check=True)
        defaults["ultimate"] = dict(host="10.0.0.79", ftp_user="anonymous",
            ftp_password="anonymous@", remote_root=remote, disk8=str(disk.resolve()),
            drive_a_bus_id=8, drive_b_bus_id=11, drive_a_type="1581",
            drive_b_type="1581", drive_a_enabled=True, drive_b_enabled=False,
            mount_mode="readwrite", boot_drive=8, capture_video_stream=True,
            stream_port=12000, stream_timeout_s=4, default_speed_mhz=1,
            warp_equivalent_mhz=8)
        # Empty disk boot performs the framework's reset/resume sequence
        # before mounting, without injecting a command or loading a PRG.
        step("reset_before_mount", "ultimate.launch", boot_mode="disk",
             boot_command="", reset_before_boot=True, post_reset_delay_s=3,
             post_resume_delay_s=7, drives=[dict(slot="a", bus_id=8,
                 drive_type="1581", enabled=True, disk="", remote_disk="")])
        step("boot_readyos", "ultimate.launch", boot_mode="disk", boot_drive=8,
            drives=[dict(slot="a", bus_id=8, drive_type="1581", enabled=True,
                disk=str(disk.resolve()), remote_disk=remote + "/RETURN.D81",
                image_type="d81", mount_mode="readwrite")],
            boot_command='LOAD"BOOT",8\r', reset_before_boot=True,
            post_reset_delay_s=3, post_resume_delay_s=7, boot_inter_chunk_delay_s=0.25)
        step("run_readyos", "input.sequence", pre_delay_s=45,
             keys=list(b"RUN\r"), inter_key_delay_s=0.25, post_delay_s=90)
        wait("initial_dma_ready", "DMA:YES")
    else:
        if not args.boot:
            parser.error("--boot is required for VICE")
        vice = dict(disk8=str(args.disk.resolve()), drive8_type=1541 if cartridge else 1581,
            drive9_enabled=False, true_drive=cartridge, close_vice=True,
            headless=True, speed_percent=100)
        if cartridge:
            vice.update(cart_crt=str(args.boot.resolve()), autostart_enabled=False)
        else:
            vice["autostart_prg"] = str(args.boot.resolve())
        defaults.update(vice=vice, monitor_host="127.0.0.1", monitor_port_start=6502,
                        monitor_port_span=40)
        step("boot_readyos", "vice.launch", kill_stale=True)
    wait("initial_menu", footer)
    editor = 0 if cartridge else 2
    for speed in ((1, 16, 64) if ultimate else (1,)):
        prefix = f"speed_{speed}"
        if ultimate:
            step(prefix, "ultimate.speed.set", mhz=speed)
            keys(prefix + "_preload", [19, 17, 17, 136, 134], delay=8)
            wait(prefix + "_loaded", "PRESS ANY KEY")
            step(prefix + "_load_ok", "assert.screen", contains="OK  -")
            keys(prefix + "_ack", [32])
            wait(prefix + "_dma_used", "DMA:ON")
            keys(prefix + "_editor", [13])
        else:
            keys(prefix + "_editor", [19] + [17] * editor + [13])
        wait(prefix + "_editor_open", "EDITOR:")
        marker = "RETURN" + str(speed)
        keys(prefix + "_edit", list(marker.encode()))
        wait(prefix + "_buffer", marker)
        for i in range(3):
            label = f"{prefix}_cycle_{i}"
            keys(label + "_ctrl_b", [2])
            wait(label + "_menu", footer)
            if ultimate:
                step(label + "_dma", "assert.screen", contains="DMA:ON")
            else:
                step(label + "_no_dma", "assert.screen_not_contains", not_contains="DMA:")
            keys(label + "_resume", [13])
            wait(label + "_preserved", marker)
        keys(prefix + "_before_shell", [2])
        wait(prefix + "_shell_menu", footer)
        keys(prefix + "_shell", [19] + [17] * (editor + 1) + [13])
        # The help hint is cold-boot-only; the chrome is present on both
        # a fresh shell and a restored session at later Ultimate speeds.
        wait(prefix + "_shell_open", "READYOS READYSHELL" if ultimate else "RUN: CAT")
        keys(prefix + "_command", list(b"VER\r"))
        wait(prefix + "_command_result", "VERSION")
        keys(prefix + "_shell_return", [2])
        wait(prefix + "_after_shell", footer)
        keys(prefix + "_cross_app", [19] + [17] * editor + [13])
        wait(prefix + "_cross_app_buffer", marker)
        keys(prefix + "_final_return", [2])
        wait(prefix + "_final_menu", footer)
    args.out.write_text(json.dumps(plan, indent=2) + "\n")


if __name__ == "__main__":
    main()
