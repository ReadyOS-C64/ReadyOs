#!/usr/bin/env python3
"""Install the native return utility and menu on the owned HDD from BASIC.

Uses C64 OPEN/CHROUT/CLOSE, then native LOAD readback of the installed utility.
The helper is installation tooling only; switching has no monitor dependency.
"""
import hashlib
import argparse
import json
from pathlib import Path
import socket
import subprocess
import sys
import time
from cap_native_reu import ENV, ROOT, Monitor, feed, wait_bytes


def validate_menu(menu):
    lines = menu.rstrip(b'\r').split(b'\r')
    def node(index):
        assert index < len(lines), 'Menu child count exceeds file'
        line = lines[index]
        if b';' not in line:
            assert line == b'*' or (b':' in line and len(line.rsplit(b':',1)[1]) == 3)
            return index+1
        title, count = line.rsplit(b';',1)
        assert title and len(count) == 1 and 65 <= count[0] <= 90
        index += 1
        for _ in range(count[0]-64):
            index = node(index)
        return index
    assert node(0) == len(lines), 'Menu contains uncounted children'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reset',action='store_true',
                        help='Reset the verified owned VICE to BASIC before installation')
    args=parser.parse_args()
    prg = (ROOT / 'bin/c64os-readyos-utility.prg').read_bytes()
    assert prg[:2] == b'\x00\xe0'
    subprocess.run(['ca65', '-o', str(ENV/'install_file.o'),
                    str(ROOT/'experiments/c64os/install_file.s')], check=True)
    cfg = ENV/'install_file.cfg'
    cfg.write_text('MEMORY { RAM: start=$C000, size=$0200, file=%O; }\n'
                   'SEGMENTS { CODE: load=RAM, type=rw; }\n')
    helper = ENV/'install_file.bin'
    subprocess.run(['ld65', '-C', str(cfg), '-o', str(helper),
                    str(ENV/'install_file.o')], check=True)
    # Preserve the baseline menu, adding one explicit experimental item.
    menu = (ENV/'baseline/os/SETTINGS/utilities.m.S00').read_bytes()[26:]
    name = b'\xd2EADY\xcf\xd3'
    # The letter after ';' encodes the immediate child count (A = 1).
    # Adding an item without incrementing H -> I corrupts menu parsing.
    assert menu.startswith(b'=;H\r')
    menu = menu.replace(b'=;H\r', b'=;I\r'+name+b':7R\x01\r', 1)
    validate_menu(menu)
    with socket.create_connection(('127.0.0.1', 6511), 5) as sock:
        sock.settimeout(20)
        mon = Monitor(sock)
        resource_name = b'IDE64Image1'
        result = mon.command(0x51, bytes([len(resource_name)])+resource_name)
        mounted = Path(result[2:2+result[1]].rstrip(b'\0').decode()).resolve()
        assert mounted == (ENV/'runtime/vice support/c64os v1.09.hdd').resolve()
        from verify_bridge_ui import wait_screen
        if args.reset:
            mon.command(0xcc,b'\x01')
            mon.command(0xaa)
        wait_screen(mon,'COMMODORE 64 BASIC V2')
        for filename, payload in [(b'@1//OS/UTILITIES/:'+name+b',P,W', prg),
                                  (b'@1//OS/SETTINGS/:UTILITIES.M,S,W', menu)]:
            mon.write(0xc000, helper.read_bytes())
            mon.write(0xc200, filename)
            mon.write(0x2000, payload)
            mon.write(0xc2f0, bytes([len(filename)])+len(payload).to_bytes(2,'little')+b'\0')
            feed(mon, b'SYS49152\r')
            deadline = time.monotonic()+30
            while time.monotonic()<deadline:
                status = mon.read(0xc2f3,1)[0]
                mon.command(0xaa)
                if status:
                    assert status == 1, f'Native write failed: {filename!r}'
                    break
                time.sleep(.1)
            else:
                raise RuntimeError('Native file write timed out')
            time.sleep(.3)
        # Call LOAD from SYS: BASIC's LOAD command relinks arbitrary PRG bytes
        # as BASIC line pointers and would corrupt a relocated readback.
        filename = b'1//OS/UTILITIES/:'+name
        mon.write(0xc200, filename)
        mon.write(0xc2f0, bytes([len(filename),0,0,0]))
        mon.write(0x2000, bytes(len(prg)-2))
        feed(mon, b'SYS49408\r')
        wait_bytes(mon, 0xc2f3, b'\x01')
        assert mon.read(0x2000,len(prg)-2) == prg[2:]
        mon.command(0xaa)
        time.sleep(.2)
        filename = b'1//OS/SETTINGS/:UTILITIES.M,S,R'
        mon.write(0xc200, filename)
        mon.write(0xc2f0, bytes([len(filename)])+len(menu).to_bytes(2,'little')+b'\0')
        mon.write(0x2000, bytes(len(menu)))
        feed(mon, b'SYS49472\r')
        wait_bytes(mon, 0xc2f3, b'\x01')
        assert mon.read(0x2000,len(menu)) == menu
        mon.command(0xaa)
    report = {'utility_sha256': hashlib.sha256(prg).hexdigest(),
              'utility_bytes':len(prg), 'native_readback_verified': True,
              'menu_validated_and_native_readback_verified': True,
              'hdd':str(mounted), 'menu_item':'ReadyOS'}
    (ENV/'native-utility-install.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__ == '__main__':
    main()
