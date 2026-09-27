#!/usr/bin/env python3
"""Fresh normal ReadyOS boot without a cartridge; no BOOTER on device 8.

Exercises the same-context transfer and ordinary IEC cold-load failure. Does
not claim full C64OS boot on an unavailable SD2IEC/CMD installation.
"""
import json
import socket
from verify_bridge_ui import ENV, BINARY_PORT, Monitor, feed, wait_screen
from verify_roundtrip_ui import launcher_app, snapshot


def main():
    with socket.create_connection(('127.0.0.1', BINARY_PORT), 5) as sock:
        sock.settimeout(20)
        mon = Monitor(sock)
        try:
            launcher_app(mon, 0)
            wait_screen(mon, 'TEST BOOT FAILURE RECOVERY (X)')
            shim = mon.read(0xc600, 1024)
            mon.command(0xaa)
            before = snapshot('noide-before')
            for n in range(1, 4):
                feed(mon, b'S')
                wait_screen(mon, f'RESTORED ZP/STACK/SHIM: {n}')
                assert mon.read(0xc600, 1024) == shim
                mon.command(0xaa)
            for key in (b'C', b'X'):
                feed(mon, key)
                wait_screen(mon, 'COLD BOOT FAILED: 4; READYOS RESTORED', timeout=60)
                assert mon.read(0xc600, 1024) == shim
                mon.command(0xaa)
            after = snapshot('noide-after')
            assert after[32*65536+0x706] == 0, 'Cartridge incorrectly detected'
            for bank in (33, 35):
                assert after[bank*65536:(bank+1)*65536] == before[bank*65536:(bank+1)*65536]
            feed(mon, [133])
            wait_screen(mon, 'APPLICATIONS')
        finally:
            mon.command(0xaa)
    report = {'pass': True, 'scope': 'VICE without IDE64, standard IEC device 8',
              'ram_proof_cycles': 3, 'missing_booter_and_failure_probe_recover': True,
              'ide64_snapshot_banks_untouched': [33, 35], 'shim_bytes_preserved': 1024}
    (ENV/'noide-ui.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
