"""Check the public release against Git's file inventory, not local build debris."""
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import subprocess

import readyos_profiles as profiles

ROOT = profiles.ROOT
MEDIA = re.compile(r"""^\s*(?:disk[89]?|autostart_prg|cart_crt|prg):\s*["']?([^"'\s#]+)""", re.M)


def main():
    tracked = set(subprocess.check_output(
        ["git", "ls-files", "-z"], cwd=ROOT).decode().rstrip("\0").split("\0"))
    failures = [f"private document tracked: {p}" for p in sorted(tracked)
                if p.startswith("privatedocs/")]

    def check(owner, value):
        p = PurePosixPath(value)
        if (PureWindowsPath(value).drive or p.is_absolute() or ".." in p.parts
                or "\\" in value or value not in tracked or not (ROOT / p).is_file()):
            failures.append(f"{owner}: not a portable, tracked input: {value}")
        return value

    release = profiles.release_version_dir(profiles.read_current_version_text())
    for pid in [*profiles.list_profile_ids(), "precog-easyflash"]:
        path = release / pid / "manifest.json"
        check(pid, path.relative_to(ROOT).as_posix())
        data = json.loads(path.read_text())
        profiles.map_manifest_paths(data, lambda val: check(pid, val))

    for name in sorted(tracked):
        if name.startswith("build_support/") and name.endswith(".generated.yaml"):
            for value in MEDIA.findall((ROOT / name).read_text()):
                if value.startswith("Releases/") or PureWindowsPath(value).drive or value.startswith("/"):
                    check(name, value)
    if failures:
        raise SystemExit("\n".join(failures))
    print("Public release manifests, tracked plan media and private-doc boundary: OK")


if __name__ == "__main__":
    main()
