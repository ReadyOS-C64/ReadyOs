#!/usr/bin/env python3
"""Build our native GEOS return accessories without external OS disks or labs."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def build(target):
    source = ROOT / 'experiments' / target
    work = ROOT / 'obj' / (target + '-accessory')
    dest = ROOT / 'assets' / target
    work.mkdir(parents=True, exist_ok=True)
    dest.mkdir(parents=True, exist_ok=True)

    def run(*args):
        subprocess.run([str(a) for a in args], cwd=ROOT, check=True)

    run('ca65', '-I', ROOT, '-o', work / 'accessory.o', source / 'accessory.s')
    run('ld65', '-C', source / 'accessory.cfg', '-Ln', work / 'accessory.lbl',
        '-m', work / 'accessory.map', '-o', work / 'accessory.bin', work / 'accessory.o')
    labels = {line.split()[2].lstrip('.'): int(line.split()[1], 16)
              for line in (work / 'accessory.lbl').read_text().splitlines()}
    payload = (work / 'accessory.bin').read_bytes()
    start, end = labels['entry'], labels['image_end']
    assert start == 0x1000 and end < 0x7c00
    directory, info = bytearray(254), bytearray(254)
    directory[0] = 0x83
    directory[3:19] = b'ReadyOS Bridge'.ljust(16, b'\xa0')
    directory[21:23] = bytes([0, 5])
    struct.pack_into('<H', directory, 28, (len(payload) + 253) // 254 + 1)
    directory[30:59] = b'SEQ formatted GEOS file V1.0\0'
    info[0:3] = bytes([3, 21, 0xbf])
    for y in range(21):
        row = 0xffffff if y in (0, 20) else (0x818181 if y < 10 else 0x81ff81)
        info[3 + y * 3:6 + y * 3] = row.to_bytes(3, 'big')
    info[0x42:0x45] = bytes([0x83, 5, 0])
    struct.pack_into('<HHH', info, 0x45, start, end, start)
    info[0x4b:0x5f] = b'ReadyOS Bridge V0.1\0'.ljust(20, b'\0')
    info[0x60:0x70] = b'ReadyOS project\0'
    description = b'Experimental ' + (b'GDOS' if target == 'gdos64' else b'GEOS') + b' / ReadyOS transition\0'
    info[0x87:0x87 + 39] = description
    assert len(directory) == len(info) == 254
    cvt = bytes(directory + info) + payload
    (dest / 'readyos-bridge.cvt').write_bytes(cvt)
    report = {'target': target, 'load': start, 'end': end,
              'cvt_sha256': hashlib.sha256(cvt).hexdigest(),
              'source': str((source / 'accessory.s').relative_to(ROOT))}
    (dest / 'bridge.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Built native accessory:', target, report['cvt_sha256'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True, choices=['gdos64', 'geos20'])
    build(parser.parse_args().target)
