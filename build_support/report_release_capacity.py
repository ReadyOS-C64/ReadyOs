#!/usr/bin/env python3
"""Report actual image free blocks and bridge payloads for a rebuilt release."""
import argparse
import json
from pathlib import Path
import re
import subprocess

from build_apps_catalog_petscii import parse_source

ROOT = Path(__file__).resolve().parents[1]


def report(version):
    rows = []
    for manifest_path in sorted((ROOT / 'Releases' / version).glob('*/manifest.json')):
        manifest = json.loads(manifest_path.read_text())
        assert manifest['version_text'] == version, manifest_path
        folder = manifest_path.parent
        catalog = ROOT / manifest['catalog_source']
        skip = int(parse_source(str(catalog))[0]['reu_bank_skip'])
        disks = []
        payloads = set()
        images = manifest.get('disks')
        if images is None:
            images = [{'drive': 8, 'path': manifest['artifacts']['data_disk']}]
        for image in images:
            path = ROOT / image['path']
            proc = subprocess.run(['c1541', str(path), '-list'], check=True,
                                  capture_output=True, text=True)
            match = re.search(r'(\d+) blocks free', proc.stdout, re.I)
            if not match:
                raise ValueError('No free-block count: ' + str(path))
            names = {m.group(1).lower() for m in re.finditer(r'"([^"]+)"\s+prg', proc.stdout, re.I)}
            bridges = sorted(names.intersection({'c64os', 'gdos64', 'geos20'}))
            payloads.update(bridges)
            free = int(match.group(1))
            disks.append({'drive': image['drive'], 'image': path.name,
                          'free_blocks': free, 'free_payload_bytes': free * 254,
                          'bridges': bridges})
        row = {'sku': manifest['id'], 'skip': skip, 'disks': disks,
               'bridge_payloads': sorted(payloads),
               'companions': sorted(p.name for p in folder.iterdir()
                                    if p.suffix in {'.car', '.cvt'})}
        if 'artifacts' in manifest:
            layout = json.loads((ROOT / manifest['artifacts']['layout']).read_text())
            row['cartridge_free_banks'] = 63 - layout['last_used_bank']
            row['cartridge_free_bytes'] = row['cartridge_free_banks'] * 16384
        rows.append(row)
    return rows


def markdown(rows):
    lines = ['| SKU | Disk free blocks (KiB of file payload) | Bridges present | Compiled skip |',
             '| --- | --- | --- | --- |']
    for row in rows:
        space = '; '.join(f"D{d['drive']}: {d['free_blocks']} ({d['free_payload_bytes']/1024:.1f})"
                          for d in row['disks'])
        if 'cartridge_free_banks' in row:
            space += f"; CRT: {row['cartridge_free_banks']} banks ({row['cartridge_free_bytes']/1024:.0f} KiB)"
        bridges = ', '.join(row['bridge_payloads']) or 'None'
        lines.append(f"| {row['sku']} | {space} | {bridges} | {row['skip']} |")
    return '\n'.join(lines) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', default='0.5')
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    rows = report(args.version)
    print(json.dumps(rows, indent=2) if args.json else markdown(rows), end='\n')
