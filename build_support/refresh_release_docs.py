#!/usr/bin/env python3
"""Refresh release help without rebuilding or relabelling existing media."""
import argparse
import json
from pathlib import Path
import subprocess

import readyos_profiles as profiles
import readyos_easyflash as easyflash

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", default="0.5")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--manifest-ref", help="Read manifests from a Git revision, keeping documentation-only commits aligned with committed media")
    args = parser.parse_args()
    directory = ROOT / "Releases" / args.release
    if directory.parent != ROOT / "Releases" or not directory.is_dir():
        raise SystemExit("Expected an existing release directory name")
    outputs = {directory / "README.md": profiles.render_release_root_readme(args.release)}
    for manifest in sorted(directory.glob("*/manifest.json")):
        if args.manifest_ref:
            manifest_text = subprocess.check_output(
                ["git", "show", f"{args.manifest_ref}:{manifest.relative_to(ROOT).as_posix()}"],
                cwd=ROOT, text=True)
        else:
            manifest_text = manifest.read_text()
        resolved = json.loads(manifest_text)
        pid = resolved['id']
        if pid == 'precog-easyflash':
            content = easyflash.build_help_text(manifest.parent)
        else:
            profile = profiles.load_profile(pid)
            entries = profiles.parse_catalog_entries(profile, None)
            content = profiles.build_help_text(profile, resolved, entries)
        for name in ('README.md', 'help.md', 'helpme.md'):
            outputs[manifest.parent / name] = content
    stale = []
    for path, content in outputs.items():
        if args.check:
            if not path.exists() or path.read_text() != content:
                stale.append(str(path.relative_to(ROOT)))
        else:
            path.write_text(content)
    if stale:
        raise SystemExit('Stale release docs: ' + ', '.join(stale))
    print(f"{'Verified' if args.check else 'Refreshed'} {len(outputs)} release documents; media/manifests unchanged")

if __name__ == '__main__':
    main()
