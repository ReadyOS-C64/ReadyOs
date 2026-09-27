#!/usr/bin/env python3
"""Verify a fresh stock D81: first-page manifest and skip-zero safety gate.

Launch the normal D81 through run.sh, with the standard catalog and skip zero.
Uses only the normal launcher, file picker and bridge UI. Leaves launcher open.
"""
import json
import socket
from verify_bridge_ui import ENV, BINARY_PORT, Monitor, feed, wait_screen, screenshot

with socket.create_connection(('127.0.0.1', BINARY_PORT), 5) as sock:
    sock.settimeout(20)
    mon = Monitor(sock)
    text = wait_screen(mon, 'APPLICATIONS')
    assert 'C64OS BRIDGE' not in text
    assert mon.read(0xc83b, 1) == b'\0'
    mon.command(0xaa)
    feed(mon, [135])
    text = wait_screen(mon, 'BROWSE APP MANIFEST')
    assert 'APP.C64OS' in text, 'Manifest must appear without scrolling'
    (ENV / 'stock-manifest-first-page.txt').write_text(text)
    screenshot('stock-manifest-first-page.png')
    # Standard config order: apps.cfg, app.sidetris, app.c64os.
    feed(mon, [17, 17, 13])
    wait_screen(mon, 'PRESS ANY KEY')
    feed(mon, b' ')
    wait_screen(mon, 'APPLICATIONS')
    feed(mon, b'\r')
    text = wait_screen(mon, 'C64OS BRIDGE SETUP REQUIRED')
    assert 'THIS READYOS BOOT SKIPS 0 BANKS' in text
    assert 'READYOS SKIP STAYS 39' in text
    (ENV / 'stock-skip-zero-gate.txt').write_text(text)
    screenshot('stock-skip-zero-gate.png')
    feed(mon, b'L')
    wait_screen(mon, 'C64OS BOOT LOCATION')
    feed(mon, b'\r')
    wait_screen(mon, 'APPLICATIONS')

report = {'pass': True, 'default_skip': 0, 'bridge_not_default_app': True,
          'manifest_visible_on_first_page': True, 'manifest_browse_position': 3,
          'loaded_via_optional_manifest': True, 'unsafe_switch_blocked': True,
          'boot_location_available_from_gate': True}
(ENV / 'stock-d81-ui.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
