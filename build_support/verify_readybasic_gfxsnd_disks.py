#!/usr/bin/env python3
"""Read back both release D81s and compare demo/runtime/resource bytes."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
files = {
    'readybasic': 'bin/readybasic.prg',
    'rbgfxsnddemo': 'obj/rbgfxsnddemo.prg',
    'rbugfxsnddemo': 'obj/rbugfxsnddemo.prg',
    'rbm.media,s': 'obj/readybasic_modules/rbm.media.seq',
    'rb.summer,s': 'assets/readybasic/music/summer-9200.sid',
    'rb.neon,s': 'assets/readybasic/neon/rb.neon.koa',
    'rb.ready,s': 'assets/readybasic/neon/rb.ready.rbr',
}
for profile in ('precog-d81', 'precog-ultimate'):
    manifest=json.loads((ROOT/f'Releases/0.5/{profile}/manifest.json').read_text())
    disk=manifest['disks'][0]['path']
    with tempfile.TemporaryDirectory(prefix='readybasic-disk-check-') as temporary:
        for name, source in files.items():
            target=Path(temporary)/name
            subprocess.run(['c1541', disk, '-read', name, str(target)],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            assert target.read_bytes()==(ROOT/source).read_bytes(), (profile, name)
        if profile=='precog-ultimate':
            target=Path(temporary)/'apps.cfg'
            subprocess.run(['c1541', disk, '-read', 'apps.cfg,s', str(target)],
                check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            cfg=target.read_bytes().decode('ascii').lower()
            deploy=json.loads((ROOT/'build/readybasic-media-tools/neon-deployment.json').read_text())
            assert disk==deploy['disk']
            assert 'c64u_image_path='+deploy['remote'].lower()+'\r' in cfg
            assert 'dma_loading=1\r' in cfg
    print('EXACT D81 CONTENT VERIFIED:', disk)
