#!/usr/bin/env python3
"""Verify the semantic directory order of every ReadyOS release disk image."""

from __future__ import annotations

import argparse
import json
import os
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
sys.path.insert(0, str(ROOT / "build_support"))

import readyos_profiles


def group_summary(disk_path: Path, overrides: dict[str, str]) -> str:
    counts: Counter[str] = Counter()
    for entry in readyos_profiles.list_disk_directory(disk_path):
        name = str(entry["name"])
        group = readyos_profiles.disk_directory_group(
            name,
            str(entry["type"]),
            overrides.get(name.lower()),
        )
        counts[group] += 1
    return ", ".join(
        f"{group}={counts[group]}"
        for group in readyos_profiles.DISK_DIRECTORY_GROUPS
        if counts[group]
    )


def audit_profile(profile_id: str) -> bool:
    try:
        profile = readyos_profiles.load_profile(profile_id)
        manifest = readyos_profiles.resolve_profile(profile_id, None, latest=True)
        catalog_entries = readyos_profiles.parse_catalog_entries(
            profile,
            str(manifest["catalog_source"]),
        )
        apps_set = readyos_profiles.enabled_apps(catalog_entries)
        disk_defs = {int(disk["index"]): disk for disk in profile["disks"]}

        print(f"PROFILE {profile_id} ({manifest['version_text']})")
        for disk in manifest["disks"]:
            disk_index = int(disk["index"])
            disk_path = Path(str(disk["path"]))
            entries = readyos_profiles.ordered_disk_entries([
                entry for entry in disk_defs[disk_index]["contents"]
                if readyos_profiles.profile_content_enabled(entry, apps_set)
            ])
            overrides = {
                str(entry["name"]).lower(): str(entry["directory_group"])
                for entry in entries
                if entry.get("directory_group") not in (None, "")
            }
            for support_entry in readyos_profiles.authoritative_support_entries(apps_set):
                if (readyos_profiles.support_target_drive(
                        profile, support_entry, catalog_entries) == int(disk["drive"]) and
                        support_entry.get("directory_group") not in (None, "")):
                    overrides[str(support_entry["disk_name"]).lower()] = str(
                        support_entry["directory_group"]
                    )
            expected_boot_names = [
                str(entry["name"])
                for entry in entries
                if readyos_profiles.disk_directory_group(
                    str(entry["name"]),
                    str(entry["type"]),
                    entry.get("directory_group"),
                ) == "boot"
            ]
            readyos_profiles.verify_disk_directory_order(
                disk_path,
                group_overrides=overrides,
                expected_boot_names=expected_boot_names,
            )
            print(
                f"  disk {disk_index} drive {disk['drive']}: {disk_path.name}: "
                f"OK ({group_summary(disk_path, overrides)})"
            )
        print("")
        return True
    except (OSError, ValueError, KeyError) as exc:
        print(f"PROFILE {profile_id}: FAIL: {exc}")
        return False


def audit_easyflash_data_disk() -> bool:
    profile_id = "precog-easyflash"
    try:
        output_dir = (
            readyos_profiles.release_version_dir(readyos_profiles.read_current_version_text())
            / profile_id
        )
        manifest_path = output_dir / "manifest.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        disk_path = Path(str(manifest["artifacts"]["data_disk"]))
        readyos_profiles.verify_disk_directory_order(disk_path)
        print(f"PROFILE {profile_id} ({manifest['version_text']})")
        print(f"  companion data disk: {disk_path.name}: OK ({group_summary(disk_path, {})})")
        print("  CRT/raw cartridge bank order is intentionally outside this disk-directory contract.")
        print("")
        return True
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"PROFILE {profile_id}: FAIL: {exc}")
        return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--profile",
        action="append",
        dest="profiles",
        help="release profile to verify; repeat for multiple profiles",
    )
    args = parser.parse_args()

    profiles = args.profiles or [*readyos_profiles.list_profile_ids(), "precog-easyflash"]
    all_ok = True
    for profile_id in profiles:
        if profile_id == "precog-easyflash":
            all_ok &= audit_easyflash_data_disk()
        else:
            all_ok &= audit_profile(profile_id)

    if not all_ok:
        print("RELEASE_DIRECTORY_ORDER_FAILED")
        return 1
    print("ALL_RELEASE_DIRECTORY_ORDERS_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
