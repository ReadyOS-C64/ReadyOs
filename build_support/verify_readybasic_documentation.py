#!/usr/bin/env python3
"""Check source coverage and rendered preservation of the ReadyBASIC reference."""
from html.parser import HTMLParser
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from build_readybasic_disk_modules import sample_inventory
from build_readme_app_assets import write_header, write_source
from readme_lite_common import parse_markdown_lite

ROOT = Path(__file__).resolve().parents[1]

class ReferenceParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.section = []
        self.rows = {}
        self.pre = None
        self.listings = []
        self.ids = set()
        self.anchors = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs:
            self.ids.add(attrs['id'])
        if tag == 'section':
            self.section.append(attrs.get('id'))
        if tag == 'tr' and self.section:
            key = self.section[-1]
            self.rows[key] = self.rows.get(key, 0) + 1
        if tag == 'pre':
            self.pre = []
        if tag == 'a' and attrs.get('href', '').startswith('#'):
            self.anchors.append(attrs['href'][1:])

    def handle_data(self, data):
        if self.pre is not None:
            self.pre.append(data)

    def handle_endtag(self, tag):
        if tag == 'section':
            self.section.pop()
        if tag == 'pre' and self.pre is not None:
            self.listings.append(''.join(self.pre).rstrip())
            self.pre = None

def main():
    subprocess.run([sys.executable, str(ROOT / 'build_support/build_readybasic_reference.py'), '--check'], check=True)
    subprocess.run([sys.executable, str(ROOT / 'build_support/render_current_docs.py'), '--check'], check=True)
    app = ROOT / 'src/apps/readybasic'
    asm = (app / 'readybasic.s').read_text().split('rb_command_descriptors:', 1)[1].split('.segment "HIDDEN"', 1)[0]
    names = re.findall(r'^\s*CMD_[A-Z0-9_]+[^\n]*"([A-Z0-9]+)"', asm, re.M)
    doc = (ROOT / 'docs/readybasic_reference.md').read_text()
    rendered = (ROOT / 'docs/readybasic_reference.html').read_text()
    parsed = ReferenceParser()
    parsed.feed(rendered)
    expected_rows = {'implemented-built-in-inventory': len(names) + 1,
                     'the-eight-on-demand-media-commands': 9,
                     'developer-sample-module-commands': 1 + sum(map(len, sample_inventory().values())),
                     'every-example-at-a-glance': len(list(app.glob('*.bas'))) + 1}
    for section, count in expected_rows.items():
        if parsed.rows.get(section) != count:
            raise SystemExit(f'Rendered table {section}: expected {count} rows, got {parsed.rows.get(section)}')
    for source in app.glob('*.bas'):
        text = source.read_text().rstrip()
        if f'```basic\n{text}\n```' not in doc or text not in parsed.listings:
            raise SystemExit(f'Missing or altered full source listing: {source.name}')
    missing = set(parsed.anchors) - parsed.ids
    if missing:
        raise SystemExit(f'Broken reference anchors: {sorted(missing)}')
    if 'class="math' in rendered:
        raise SystemExit('BASIC dollar signs were incorrectly interpreted as math')
    pages = parse_markdown_lite((ROOT / 'src/apps/readme/readme_lite.md').read_text(), 38, 18)
    if any(len(page) != 18 or any(len(line.text) > 38 for line in page) for page in pages):
        raise SystemExit('Read.Me content exceeds its 38-column / 18-line page geometry')
    with tempfile.TemporaryDirectory(prefix='readyos-readme-doc-check-') as tmp:
        header, source = Path(tmp) / 'readme_pages.h', Path(tmp) / 'readme_pages.c'
        write_header(header, len(pages), 18, 38)
        write_source(source, pages, 18, 38, header.name)
        for file in (header, source):
            if file.read_bytes() != (ROOT / 'src/generated' / file.name).read_bytes():
                raise SystemExit(f'Stale generated Read.Me data: {file.name}; rebuild through run.sh')
    print(f'ReadyBASIC documentation verified: {len(names)} built-ins, 8 media, 49 distinct sample commands, '
          f'{len(list(app.glob("*.bas")))} verbatim source listings and all internal anchors')
    print(f'Read.Me generated content verified: {len(pages)} pages, 38 columns, 18 lines per page')

if __name__ == '__main__':
    main()
