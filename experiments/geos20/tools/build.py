import os
"""Build the native accessory and install it into private, backed-up lab media."""
from pathlib import Path
import subprocess,struct,shutil,json,hashlib
SOURCE=Path(__file__).resolve().parents[1]
READY=SOURCE.parents[1]
ROOT=Path(os.environ.get('GEOS_BRIDGE_PROJECT',str(READY.parent/'GEOS20')))
# READY comes from the source checkout above.
B=ROOT/'build'
for name in ('build','media','runtime','evidence'):(ROOT/name).mkdir(exist_ok=True)
assert 'READYOS_REU_BANK_SKIP = 71' in (READY/'src/generated/readyos_reu_config.inc').read_text(), 'Build ReadyOS experimental skip71 profile first'
shutil.copy2(READY/'Releases/0.5/precog-geos20/readyos-v0.5-d81.d81',ROOT/'media/readyos.d81')

def run(*args):return subprocess.run([str(a) for a in args],cwd=READY,check=True,text=True)
run('python3',READY/'build_support/build_geos_accessory.py','--target','geos20')
cvt=ROOT/'build/ReadyOS Bridge.cvt'
shutil.copy2(READY/'assets/geos20/readyos-bridge.cvt',cvt)
labels=json.loads((READY/'assets/geos20/bridge.json').read_text())
start=labels['load'];end=labels['end']
# Only owned copies are rewritten; original lab and interactive disks untouched.
target=ROOT/'media/geos-system.d81'
shutil.copy2(ROOT/'media/geos20-original.d81',target)
run('c1541','-attach',target,'-geoswrite',cvt)
(ROOT/'build/manifest.json').write_text(json.dumps({'load':start,'end':end,'cvt_sha256':hashlib.sha256(cvt.read_bytes()).hexdigest()},indent=2)+'\n')
print('Native accessory:',cvt,'RAM',hex(start),'-',hex(end))
