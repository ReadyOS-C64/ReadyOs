#!/usr/bin/env python3
"""Verify the ReadyOS app rejects a 4MB REU from a normal launcher boot."""
import json
import socket
from verify_roundtrip_ui import launcher_app
from verify_bridge_ui import ENV, Monitor, feed, screenshot, wait_screen

with socket.create_connection(('127.0.0.1',6511),5) as sock:
    sock.settimeout(10)
    mon=Monitor(sock)
    name=b'REUsize'
    value=mon.command(0x51,bytes([len(name)])+name)
    assert value==b'\x01\x04\x00\x10\x00\x00','This test requires the actual 4MB VICE configuration'
    mon.command(0xaa)
    launcher_app(mon,0)
    text=wait_screen(mon,'REQUIRES SKIP 39 AND 16MB REU')
    assert 'RESTORED ZP/STACK/SHIM:' not in text
    shot=screenshot('rejected-4mb.png')
    feed(mon,b'C')
    wait_screen(mon,'APPLICATIONS')
report={'pass':True,'unsupported_reu_mb':4,'returned_to_launcher':True,'screenshot':shot}
(ENV/'rejected-4mb.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
