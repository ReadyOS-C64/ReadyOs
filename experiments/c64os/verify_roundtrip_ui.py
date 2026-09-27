#!/usr/bin/env python3
"""Exercise a fresh normal ReadyOS boot, both native UIs and real disk I/O.

ReadyOS input uses VICE's keyboard feed. C64 OS input uses its documented mouse
and command event queues, consumed by the normal event loop/menu dispatcher.
No app functions, PC registers, or machine state are injected/restored. VSFs
are read-only evidence for REU comparisons, never the switching mechanism.
"""
import hashlib
import json
import re
import socket
import struct
import sys
import time
from pathlib import Path
from verify_bridge_ui import ENV, ROOT, BINARY_PORT, TEXT_PORT, Monitor, feed, screen, screenshot, wait_screen
from cap_native_reu import wait_bytes

TEXT = b'BRIDGE STATE SEPTEMBER 25'
FILENAME = b'BRG25TEST'


def native_command(mon, char=b'R', mods=7):
    assert mon.read(0x311,1) == b'\0', 'Native command queue is busy'
    mon.write(0x7fa,char)
    mon.write(0x7fd,bytes([mods]))
    mon.write(0x311,b'\1')
    mon.command(0xaa)


def mouse(mon, x, y, kinds):
    assert len(kinds) <= 6
    assert mon.read(0x310,1) == b'\0', 'Native mouse queue is busy'
    mon.write(0x7e8,bytes([x])*len(kinds))
    mon.write(0x7ee,bytes([y])*len(kinds))
    mon.write(0x7f4,bytes(kinds))
    mon.write(0x310,bytes([len(kinds)]))
    mon.command(0xaa)


def snapshot(name):
    sys.path.insert(0,str(ROOT.parent/'agenticdevharness/tools'))
    from vice_readyshell_automation import Monitor as TextMonitor
    path=ENV/(name+'.vsf')
    mon=TextMonitor('127.0.0.1',TEXT_PORT)
    try:
        mon.cmd(f'dump "{path}"\n')
        mon.cmd('x\n')
    finally:
        mon.close()
    data=path.read_bytes()
    pos=58
    while pos<len(data):
        label=data[pos:pos+16].split(b'\0')[0]
        size=struct.unpack_from('<I',data,pos+18)[0]
        assert size>=22
        if label==b'REU1764':
            assert data[pos+16:pos+18]==b'\0\0'
            ram=data[pos+42:pos+size]
            assert len(ram)==16*1024*1024
            return ram
        pos+=size
    raise AssertionError('Missing REU snapshot module')


def launcher_app(mon,index):
    wait_screen(mon,'APPLICATIONS')
    feed(mon,[19]+[17]*(index+2)+[13])


def d81_view(mon):
    previous=None
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        text=screen(mon)
        body=text.splitlines()[2:24]
        mon.command(0xaa)
        # The status bar reports the most recent I/O device, which becomes 12
        # when ReadyOS utility loads. The selected device and directory remain
        # device 8; compare that actual view instead of the last-I/O status.
        if 'OFFLINE' in text and 'SYSINFO' in text and 'UCITEST' in text:
            if body==previous:
                return body[:16]
            previous=body
        time.sleep(.1)
    raise AssertionError('Native D81 directory did not finish rendering:\n'+text)


def main():
    report={'pass':False,'scope':'quiet text mode, IDE64 v4.1, fixed 16MB REU',
            'native_input':'C64 OS event queues dispatched by its normal UI',
            'screenshots':[]}
    with socket.create_connection(('127.0.0.1',BINARY_PORT),5) as sock:
        sock.settimeout(10)
        mon=Monitor(sock)
        launcher_app(mon,0)
        wait_screen(mon,'TEST BOOT FAILURE RECOVERY (X)')
        shim=mon.read(0xc600,1024)
        video_standard=mon.read(0x02a6,1)
        assert shim[0x23b]==39
        mon.command(0xaa)
        for n in range(1,4):
            feed(mon,b'S')
            wait_screen(mon,f'RESTORED ZP/STACK/SHIM: {n}')
            assert mon.read(0xc600,1024)==shim
            mon.command(0xaa)
        report['destructive_ram_proof_cycles']=3
        feed(mon,b'X')
        wait_screen(mon,'COLD BOOT FAILED: 4; READYOS RESTORED')
        assert mon.read(0xc600,1024)==shim
        mon.command(0xaa)
        report['missing_boot_file_recovers_with_kernal_error']=4
        report['screenshots'].append(screenshot('roundtrip-failed-boot-recovered.png'))
        for n in range(4,8):
            feed(mon,b'C')
            wait_screen(mon,'DEVICES')
            wait_screen(mon,'OFFLINE')
            assert mon.read(0x0281,1)==b'\x20'
            # Native event injection bypasses ROM key enqueue; a disabled
            # keyboard buffer otherwise lets this demo pass with typing broken.
            assert 0 < mon.read(0x0289,1)[0] <= 10, 'Printable keyboard input disabled or unsafe'
            assert mon.read(0x02a6,1) == video_standard, 'PAL/NTSC flag changed'
            expected_timing = bytes.fromhex('32211911' if video_standard[0] else '3c281e14')
            assert mon.read(0xeb,4) == expected_timing, 'Wrong native timing table'
            if n == 4:
                assert mon.read(0x028b,1) == b'\x04', 'Missing initial key-repeat countdown'
            assert mon.read(0x03fe,1)==b'\0', 'Utility did not close after resume'
            mon.command(0xaa)
            if n==4:
                report['screenshots'].append(screenshot('roundtrip-cold-desktop.png'))
            native_command(mon)
            wait_screen(mon,f'RESTORED ZP/STACK/SHIM: {n}')
            assert mon.read(0xc600,1024)==shim
            mon.command(0xaa)
        report['cold_round_trips']=1
        report['warm_round_trips_before_workload']=3
        low_before=snapshot('roundtrip-before-readyos-work')[:32*65536]
        feed(mon,[133])
        launcher_app(mon,1)
        wait_screen(mon,'EDITOR:@UNTITLED')
        feed(mon,TEXT)
        wait_screen(mon,TEXT.decode())
        feed(mon,[2])
        launcher_app(mon,0)
        wait_screen(mon,'TEST BOOT FAILURE RECOVERY (X)')
        before_native=snapshot('roundtrip-before-c64os-work')
        assert before_native[:32*65536]==low_before,'ReadyOS changed C64 OS REU banks'
        report['c64os_reu_bytes_preserved_during_readyos_work']=len(low_before)
        feed(mon,b'C')
        wait_screen(mon,'DEVICES')
        # Open the real attached ReadyOS D81 through C64 OS File Manager.
        wait_bytes(mon,0x3fe,b'\0')
        mon.command(0xaa)
        mouse(mon,24,28,[1,3,4,1,3,5])
        native_view=d81_view(mon)
        report['screenshots'].append(screenshot('roundtrip-c64os-reads-d81.png'))
        # Exercise the visible hamburger menu, not only its command shortcut.
        mouse(mon,1,4,[1])
        wait_screen(mon,'READYOS')
        report['screenshots'].append(screenshot('roundtrip-native-readyos-menu.png'))
        mouse(mon,10,12,[2,3])
        wait_screen(mon,'RESTORED ZP/STACK/SHIM: 1')
        after_native=snapshot('roundtrip-after-c64os-work')
        assert after_native[39*65536:]==before_native[39*65536:], \
            'C64 OS changed ReadyOS control or dynamic REU banks'
        report['readyos_reu_bytes_preserved_during_c64os_work']=len(after_native)-39*65536
        feed(mon,[133])
        launcher_app(mon,1)
        text=wait_screen(mon,TEXT.decode())
        assert '*MODIFIED*' in text
        report['unsaved_editor_text_preserved']=True
        feed(mon,[135])
        wait_screen(mon,'SAVE FILE')
        feed(mon,FILENAME+b'\r')
        text=wait_screen(mon,'EDITOR:@'+FILENAME.decode())
        assert '*MODIFIED*' not in text
        # Reopen the written file from the native directory picker. This proves
        # disk content, independently of the editor's in-memory saved flag.
        feed(mon,[136])
        text=wait_screen(mon,'OPEN FILE')
        count=int(re.search(r'(\d+)FILE\(S\)',text)[1])
        feed(mon,[17]*(count-1))
        wait_screen(mon,']> '+FILENAME.decode(),timeout=60)
        feed(mon,[13])
        text=wait_screen(mon,TEXT.decode())
        assert 'EDITOR:@'+FILENAME.decode() in text
        report['native_d81_save_and_reopen_verified']=True
        report['screenshots'].append(screenshot('roundtrip-editor-disk-readback.png'))
        low_after_disk=snapshot('roundtrip-after-readyos-disk')[:32*65536]
        assert low_after_disk==after_native[:32*65536]
        feed(mon,[2])
        launcher_app(mon,0)
        wait_screen(mon,'TEST BOOT FAILURE RECOVERY (X)')
        feed(mon,b'C')
        wait_screen(mon,'DEVICES')
        assert d81_view(mon)==native_view,'Native File Manager view changed'
        report['native_file_manager_location_and_view_preserved']=True
        native_command(mon)
        wait_screen(mon,'RESTORED ZP/STACK/SHIM: 1')
        # Failed cold attempt must leave the existing warm C64 OS context usable.
        feed(mon,b'X')
        wait_screen(mon,'COLD BOOT FAILED: 4; READYOS RESTORED')
        feed(mon,b'C')
        wait_screen(mon,'DEVICES')
        assert d81_view(mon)==native_view
        native_command(mon)
        wait_screen(mon,'RESTORED ZP/STACK/SHIM: 2')
        report['failed_cold_probe_preserves_existing_warm_context']=True
        report['warm_round_trips_total']=6
        report['screenshots'].append(screenshot('roundtrip-final-readyos.png'))
        feed(mon,[133])
        wait_screen(mon,'APPLICATIONS')
    report['pass']=True
    report['source_hashes']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
        for p in [ROOT/'bin/c64os.prg',ROOT/'bin/c64os-readyos-utility.prg']}
    (ENV/'roundtrip-ui.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':
    main()
