#!/usr/bin/env python3
"""Build from sibling source when present; otherwise verify the shipped CAR."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import binascii
ROOT=Path(__file__).resolve().parents[1]
SOURCE=Path(os.environ.get('C64OS_BRIDGE_SOURCE',ROOT.parent/'c64os-bridge'))
DEST=ROOT/'assets/c64os'
expected=hashlib.sha256((ROOT/'bin/c64os-readyos-utility.prg').read_bytes()).hexdigest()
if (SOURCE/'tools/build.py').is_file():
 subprocess.run([sys.executable,str(SOURCE/'tools/build.py'),'--readyos',str(ROOT)],check=True)
 DEST.mkdir(parents=True,exist_ok=True)
 shutil.copyfile(SOURCE/'dist/bridge.car',DEST/'bridge.car')
 report=json.loads((SOURCE/'build/report.json').read_text())
 (DEST/'bridge.json').write_text(json.dumps(report,indent=2)+'\n')
else:
 report=json.loads((DEST/'bridge.json').read_text())
 if report['utility_sha256']!=expected:
  raise SystemExit('Bridge CAR is stale: obtain the c64os-bridge source and rebuild it.')
car=(DEST/'bridge.car').read_bytes()
assert hashlib.sha256(car).hexdigest()==report['car_sha256']
assert report['utility_sha256']==expected
assert car[:12]==b'\x02\xc364\xc1RCHIVE\x03'
assert int.from_bytes(car[-4:],'little')==binascii.crc32(car[:-4])
print('Verified D81 companion bridge.car and return-utility provenance')
