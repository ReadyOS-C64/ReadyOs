#!/usr/bin/env python3
"""Upload the prepared multimedia-demo image and start normal ReadyOS disk boot.

Run from a Terminal-owned/background shell: Codex foreground networking can
report false connection failures on this host. Only a fresh owned folder is
created; no existing Ultimate image is overwritten.
"""
from ftplib import FTP
import hashlib
import io
import json
import os
from pathlib import Path
import time
from urllib.parse import urlencode
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/readybasic-media-tools"
deployment = json.loads((OUT / "neon-deployment.json").read_text())
disk = Path(deployment["disk"])
remote = deployment["remote"]
assert remote.startswith("/USB1/automation/readybasic-media/neon-")
host = os.environ.get("C64U_HOST", "10.0.0.79")


def api(path, method="GET"):
    with urlopen(Request(f"http://{host}/v1/{path}", method=method), timeout=30) as response:
        result = response.read()
    if method == "PUT":
        parsed = json.loads(result)
        assert not parsed.get("errors"), parsed
    return result


(OUT / "neon-drives-before.json").write_bytes(api("drives"))
data = disk.read_bytes()
with FTP(host, timeout=90) as ftp:
    ftp.login("anonymous", "anonymous@")
    parent, filename = remote.rsplit("/", 1)
    grandparent, folder = parent.rsplit("/", 1)
    ftp.cwd(grandparent)
    if os.environ.get('READYBASIC_REUSE_VERIFIED_IMAGE') != '1':
        ftp.mkd(folder)  # Fail if it exists; never replace another image.
        ftp.cwd(folder)
        ftp.storbinary("STOR " + filename, io.BytesIO(data))
    else:
        ftp.cwd(folder)  # Recovery boot: read back, never rewrite the image.
    readback = io.BytesIO()
    ftp.retrbinary("RETR " + filename, readback.write)
    assert readback.getvalue() == data, "uploaded D81 readback mismatch"
action = 'Verified existing image:' if os.environ.get('READYBASIC_REUSE_VERIFIED_IMAGE') == '1' else 'Uploaded and verified:'
print(action, remote, hashlib.sha256(data).hexdigest(), flush=True)
api("drives/a:mount?" + urlencode(dict(image=remote, type="d81", mode="unlinked")), "PUT")
api("machine:reset", "PUT")
time.sleep(4)


def keybuf(chunk):
    assert len(chunk) <= 10
    api("machine:writemem?address=00C5&data=0000", "PUT")
    api("machine:writemem?address=0277&data=" + chunk.hex(), "PUT")
    api("machine:writemem?address=00C6&data=" + bytes([len(chunk)]).hex(), "PUT")


# Normal PREBOOT/BOOT/launcher path. Do not poll REST while IEC is loading.
keybuf(b'LOAD "*",8')
time.sleep(1)
keybuf(b",1\r")
time.sleep(12)
keybuf(b"RUN\r")
print("Normal ReadyOS boot started; allow disk loading to finish before testing.", flush=True)
(OUT / "neon-deploy-sent.json").write_text(json.dumps(deployment, indent=2) + "\n")
