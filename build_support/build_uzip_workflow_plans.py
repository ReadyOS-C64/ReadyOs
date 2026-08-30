#!/usr/bin/env python3
"""Generate physical C64 Ultimate plans for the complete uZIP UI workflow."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def keys(text: str) -> list[int]:
    return [ord(character) for character in text]


def common(host: str, remote_root: str, port: int, speed_mhz: int) -> dict:
    step_timeout = 1800 if speed_mhz == 1 else 1200
    return {
        "retry_policy": {"max_attempts": 1, "backoff_ms": 500, "jitter": False},
        "timeouts": {"launch_s": 180, "step_s": step_timeout, "read_s": 60},
        "artifact_policy": {
            "capture_screen": True,
            "capture_state": True,
            "capture_dump": True,
        },
        "ultimate": {
            "host": host,
            "password": None,
            "ftp_user": "anonymous",
            "ftp_password": "anonymous@",
            "remote_root": remote_root,
            "disk8": None,
            "disk9": None,
            "autostart_prg": None,
            "drive_a_bus_id": 8,
            "drive_b_bus_id": 11,
            "drive_a_type": "1581",
            "drive_b_type": "1581",
            "drive_a_enabled": True,
            "drive_b_enabled": False,
            "mount_mode": "readwrite",
            "boot_drive": 8,
            "capture_video_stream": True,
            "stream_port": port,
            "stream_timeout_s": 4.0,
            "default_speed_mhz": speed_mhz,
            "warp_equivalent_mhz": speed_mhz,
        },
    }


def wait(step_id: str, text: str, timeout: int = 180, pre: float = 0.0) -> dict:
    return {
        "id": step_id,
        "type": "screen.wait_contains",
        "params": {
            "text": text,
            "wait_timeout_s": timeout,
            "poll_s": 1.0,
            "pre_delay_s": pre,
            "capture_label": step_id,
        },
    }


def sequence(
    step_id: str,
    values: list[int],
    post: float = 0.8,
    pre: float = 0.0,
) -> dict:
    return {
        "id": step_id,
        "type": "input.sequence",
        "params": {
            "keys": values,
            "pre_delay_s": pre,
            "inter_key_delay_s": 0.08,
            "post_delay_s": post,
        },
    }


def type_folder_steps(step_id: str, path: str) -> list[dict]:
    # F3 opens the path field; five DELs remove its initial /usb1 value.
    return [
        sequence(f"{step_id}_open", [134]),
        wait(f"{step_id}_prompt", "ABSOLUTE FOLDER PATH", 60),
        sequence(
            f"{step_id}_enter",
            [20, 20, 20, 20, 20] + keys(path) + [13],
            1.5,
        ),
    ]


def type_archive_name_steps(step_id: str, name: str) -> list[dict]:
    # The field starts with "archive.zip" (11 characters).
    return [
        wait(f"{step_id}_prompt", "ARCHIVE NAME", 60),
        sequence(
            f"{step_id}_enter",
            [20] * 11 + keys(name) + [13],
            1.0,
        ),
    ]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    parser.add_argument("--remote-root", required=True)
    parser.add_argument("--stream-port", type=int, required=True)
    parser.add_argument("--speed-mhz", type=int, choices=(1, 16, 64), default=16)
    parser.add_argument("--boot-output", type=Path, required=True)
    parser.add_argument("--workflow-output", type=Path, required=True)
    args = parser.parse_args()

    defaults = common(args.host, args.remote_root, args.stream_port, args.speed_mhz)
    slow = args.speed_mhz == 1
    boot_quiet_s = 900.0 if slow else 180.0
    boot_wait_s = 600 if slow else 360
    # The packed cold start takes about 14 seconds at 16 MHz and scales with
    # CPU speed.  At 1 MHz the screen workspace is still occupied after the
    # old 180-second bound, so keep REST quiet long enough for inflation to
    # finish before looking for the real home-screen success marker.
    uzip_home_wait_s = 360 if slow else 180
    create_quiet_s = 120.0 if slow else 0.0
    create_wait_s = 90 if slow else 30
    extract_quiet_s = 900.0 if slow else 60.0
    extract_wait_s = 90 if slow else 30
    app_load_quiet_s = 180.0 if slow else 60.0
    app_load_wait_s = 120 if slow else 60
    boot = {
        "version": 1,
        "kind": "ultimate_task_plan",
        "plan_id": f"uzip_complete_workflow_boot_{args.speed_mhz}mhz",
        "run_mode": "ultimate64",
        "global_defaults": defaults,
        "steps": [
            {
                "id": "launch_readyos",
                "type": "ultimate.launch",
                "params": {
                    "boot_mode": "disk",
                    "drives": [{
                        "slot": "a", "bus_id": 8, "drive_type": "1581",
                        "enabled": True, "disk": "", "remote_disk": "",
                        "image_type": "d81", "mount_mode": "readwrite",
                    }],
                    "boot_drive": 8,
                    "boot_command": "LOAD\"BOOT\",8\r",
                    "reset_before_boot": True,
                    "post_reset_delay_s": 3.0,
                    "boot_inter_chunk_delay_s": 0.25,
                },
            },
            # Physical Ultimate IEC loads must remain REST-quiet.  Earlier
            # hardware runs proved that polling during either LOAD "boot",8
            # or LOAD "launcher",8 can suspend the KERNAL load indefinitely.
            sequence("run_boot", keys("RUN\r"), 0.5, 45.0),
            wait("wait_launcher", "APPLICATIONS:", boot_wait_s, boot_quiet_s),
            # The two launcher actions (Browse and Load All) precede the app
            # catalog, so Ultimate zip is five moves below the initial row.
            sequence("select_ultimate_zip", [17, 17, 17, 17, 17, 13], 1.0),
            wait("wait_uzip_home", "ZIP CORE READY", uzip_home_wait_s),
        ],
    }

    storage_relative = (args.remote_root[5:]
                        if args.remote_root.upper().startswith("USB1/")
                        else args.remote_root)
    base = f"/usb1/{storage_relative}"
    source_dir = f"{base}/SOURCE"
    current_source_dir = f"{source_dir}/TREE"
    output_dir = f"{base}/OUT"
    destination_dir = f"{base}/DEST"
    workflow = {
        "version": 1,
        "kind": "ultimate_task_plan",
        "plan_id": f"uzip_complete_create_extract_{args.speed_mhz}mhz",
        "run_mode": "ultimate64",
        "global_defaults": defaults,
        "steps": [
            {
                "id": "attach_readyos",
                "type": "ultimate.launch",
                "params": {
                    "boot_mode": "none",
                    "drives": [{
                        "slot": "a", "bus_id": 8, "drive_type": "1581",
                        "enabled": True, "disk": "", "remote_disk": "",
                        "image_type": "d81", "mount_mode": "readwrite",
                    }],
                    "boot_drive": 8,
                },
            },
            wait("ready_before_create", "ZIP CORE READY", 60),
            # First prove the documented no-marks path and method-0 selector:
            # F1 archives the current TREE folder as one recursive seed.
            sequence("choose_store_create", [13]),
            wait("store_source_browser", "CREATE: MARK SOURCES", 60),
            *type_folder_steps("store_type_source_folder", current_source_dir),
            wait("store_current_folder_visible", "ROOT.TXT", 60),
            sequence("store_use_current_folder", [133]),
            wait("store_output_browser", "CREATE: OUTPUT FOLDER", 60),
            *type_folder_steps("store_type_output_folder", output_dir),
            *type_archive_name_steps("store_archive_name", "store.zip"),
            wait("store_method", "ZIP METHOD", 60),
            sequence("select_store_method", [145, 13]),
            wait("confirm_store_create", "CREATE ZIP?", 60),
            sequence("start_store_create", [13], 1.0),
            wait("store_create_complete", "CREATE COMPLETE", create_wait_s,
                 create_quiet_s),
            # Queue RUN/STOP with the confirmation key. The workflow opens its
            # unique temp, reaches the first safe member boundary, consumes
            # the queued cancel, deletes only that temp, and returns home.
            sequence("choose_cancel_create", [13]),
            wait("cancel_source_browser", "CREATE: MARK SOURCES", 60),
            *type_folder_steps("cancel_type_source_folder", source_dir),
            wait("cancel_source_set_visible", "LOOSE.BIN", 60),
            sequence("cancel_mark_file_and_folder", [32, 17, 32, 133], 1.5),
            wait("cancel_output_browser", "CREATE: OUTPUT FOLDER", 60),
            *type_folder_steps("cancel_type_output_folder", output_dir),
            *type_archive_name_steps("cancel_archive_name", "cancel.zip"),
            wait("cancel_method", "ZIP METHOD", 60),
            sequence("accept_cancel_compress", [13]),
            wait("confirm_cancel_create", "CREATE ZIP?", 60),
            sequence("start_and_queue_cancel", [13, 3], 1.0),
            wait("create_cancelled", "CREATE CANCELLED", 60),
            # Finally run the complete marked-sibling Deflate regression.
            sequence("choose_create", [13]),
            wait("source_browser", "CREATE: MARK SOURCES", 60),
            *type_folder_steps("type_source_folder", source_dir),
            wait("source_set_visible", "LOOSE.BIN", 60),
            # The owned source folder contains exactly two top-level entries.
            # Mark both regardless of whether Ultimate DOS returns the file or
            # folder first, then accept the sibling set with F1.
            sequence("mark_file_and_folder", [32, 17, 32, 133], 1.5),
            wait("output_browser", "CREATE: OUTPUT FOLDER", 60),
            *type_folder_steps("type_output_folder", output_dir),
            wait("archive_name", "ARCHIVE NAME", 60),
            sequence("accept_archive_name", [13]),
            wait("create_method", "ZIP METHOD", 60),
            # COMPRESS is the default selection; accept it for the recursive
            # marked-source regression. A focused companion workflow covers
            # current-folder Store and safe cancellation.
            sequence("accept_compress_method", [13]),
            wait("confirm_create", "CREATE ZIP?", 60),
            sequence("start_create", [13], 1.0),
            # Keep the 1 MHz member observation-free for the scaled compute
            # window. At 16/64 MHz a 1.5K create normally finishes in seconds.
            wait("create_complete", "CREATE COMPLETE", create_wait_s,
                 create_quiet_s),
            sequence("choose_extract", [17, 13]),
            wait("archive_browser", "EXTRACT: CHOOSE ZIP", 60),
            *type_folder_steps("type_archive_folder", output_dir),
            wait("archive_visible", "ARCHIVE.ZIP", 60),
            sequence("select_archive", [13]),
            wait("destination_browser", "EXTRACT: DESTINATION", 60),
            *type_folder_steps("type_destination_folder", destination_dir),
            wait("confirm_extract", "EXTRACT ZIP?", 60),
            sequence("start_extract", [13], 1.0),
            # The promoted extraction probes never observe REST while UCI/DOS
            # owns the machine. Preserve that discipline in the full UI flow,
            # then use a short assertion window to capture either result.
            wait("extract_complete", "EXTRACT COMPLETE", extract_wait_s,
                 extract_quiet_s),
            # COMPLETE is emitted only after uzwk/uzct bank types and owned
            # resource records have both been verified clear. Now exercise
            # the normal ReadyOS snapshot lifecycle: return to the launcher,
            # load the preceding SimpleFiles app, return through its shared
            # launcher hotkey, and relaunch the already-loaded uZIP row. The
            # preserved result text proves warm resume rather than a cold UI
            # re-entry. Keep F2/F4 app cycling out of this acceptance edge:
            # it is convenient navigation, but the launcher path is the
            # authoritative ReadyOS lifecycle and exposes the loaded marker.
            sequence("return_to_launcher", [3]),
            wait("launcher_after_extract", "APPLICATIONS:", 60),
            sequence("select_and_load_simplefiles", [145, 13], 0.0),
            # Keep REST silent while the KERNAL loads SimpleFiles from disk.
            wait("simplefiles_loaded", "SIMPLE FILES", app_load_wait_s,
                 app_load_quiet_s),
            sequence("simplefiles_return_to_launcher", [2]),
            wait("launcher_with_both_apps", "APPLICATIONS:", 60),
            sequence("relaunch_loaded_uzip", [17, 13]),
            wait("uzip_warm_result_preserved", "EXTRACT COMPLETE", 60),
            {
                "id": "capture_final_screen",
                "type": "screen.capture",
                "params": {"label": "uzip_complete_workflow_warm_pass"},
            },
        ],
    }

    args.boot_output.parent.mkdir(parents=True, exist_ok=True)
    args.boot_output.write_text(json.dumps(boot, indent=2) + "\n", encoding="utf-8")
    args.workflow_output.write_text(
        json.dumps(workflow, indent=2) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
