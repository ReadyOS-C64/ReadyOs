#!/usr/bin/env python3
"""From fresh BASIC without a bridge snapshot, test native utility rejection.

Prepare with the experiment wrapper and installer --reset; do not enter the
ReadyOS bridge first. Native C64 OS is booted only in the copied HDD environment.
"""
import json
import re
import socket
import struct
from verify_roundtrip_ui import native_command, snapshot
from verify_bridge_ui import ENV, ROOT, Monitor, feed, wait_screen, screen, screenshot

with socket.create_connection(('127.0.0.1',6511),5) as sock:
    sock.settimeout(10)
    mon=Monitor(sock)
    wait_screen(mon,'COMMODORE 64 BASIC V2')
    feed(mon,b'LOAD"1//:C64OS",12\rRUN\r')
    wait_screen(mon,'DEVICES')
    wait_screen(mon,'OFFLINE')
    before=snapshot('guard-before-utility')
    offset=39*65536+0xfd40
    expected_record=before[offset:offset+8]
    assert expected_record[:5]!=b'RBG4\x04','Requires fresh, absent bridge record'
    native_command(mon)
    # Wait for the installed utility to be loaded and its DMA guard to run.
    bss=int(re.search(r'^BSS\s+([0-9A-F]+)',
        (ROOT/'obj/c64os_readyos_utility.map').read_text(),re.M)[1],16)
    # Header is under KERNAL ROM: use the monitor's documented RAM bank.
    expected=(ROOT/'bin/c64os-readyos-utility.prg').read_bytes()[2:14]
    import time
    deadline=time.monotonic()+20
    while time.monotonic()<deadline:
        raw=mon.command(1,b'\0'+struct.pack('<HHBH',0xe000,0xe00b,0,1))[2:]
        record=mon.command(1,b'\0'+struct.pack('<HHBH',bss,bss+7,0,1))[2:]
        closed=mon.read(0x3fe,1)==b'\0'
        mon.command(0xaa)
        if raw==expected and record==expected_record and closed:
            break
        time.sleep(.1)
    else:
        raise AssertionError('Native guard did not reject missing snapshot')
    wait_screen(mon,'DEVICES')
    after=snapshot('guard-after-utility')
    assert after[32*65536:]==before[32*65536:]
    shot=screenshot('guard-rejected-missing-snapshot.png')
report={'pass':True,'missing_snapshot_rejected':True,
        'native_utility_loaded_and_returned_closed':True,
        'reu_bytes_unchanged_outside_native_partition':224*65536,
        'screenshot':shot}
(ENV/'utility-guard.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
