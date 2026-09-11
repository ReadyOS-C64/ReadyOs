#!/usr/bin/env python3
"""Reproduce the licensed, fixed-address PSID; normal builds use the checked-in SID."""
import hashlib
from pathlib import Path
import subprocess
import tarfile
import tempfile
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/readybasic/music"
def checked(data, digest):
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError("upstream checksum mismatch")
    return data

def main():
    checked((ASSETS / "shiru-originals.zip").read_bytes(),
            "636c6e00054c48cb5ac52609355e3156e97a8b890607800232c6938beb284b87")
    url = "https://hd0.linusakesson.net/files/sidreloc-1.0.tgz"
    archive = checked(urllib.request.urlopen(url, timeout=30).read(),
            "8ca55fb4886bda2a499f837e2f9ffd0a4b7217ee7bb1907ceed9e87ef6157bf6")
    with tempfile.TemporaryDirectory(prefix="readybasic-sid-") as work:
        work = Path(work)
        tar = work / "sidreloc.tgz"
        tar.write_bytes(archive)
        with tarfile.open(tar) as source:
            source.extractall(work, filter="data")
        source = work / "sidreloc-1.0"
        exe = work / "sidreloc"
        subprocess.run(["cc", "-O2", "-std=c99", "-DNDEBUG", "-o", str(exe),
            *[str(source / f) for f in ("sidreloc.c", "solver.c", "cpu.c")], "-lm"], check=True)
        original = work / "summer.sid"
        with zipfile.ZipFile(ASSETS / "shiru-originals.zip") as songs:
            original.write_bytes(songs.read("sid/summer_vacation.sid"))
        output = work / "summer-9200.sid"
        subprocess.run([str(exe), "-p", "92", "-r", "10-1a", "-z", "fc-fd",
                        "-s", "-t", "0", str(original), str(output)], check=True)
        result = checked(output.read_bytes(),
            "aa70c84615f8cadfae1edd1dc60a5250fd84c2cc412542059c509345544062aa")
        (ASSETS / output.name).write_bytes(result)

if __name__ == "__main__":
    main()
