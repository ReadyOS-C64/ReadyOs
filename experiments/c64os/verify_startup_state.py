#!/usr/bin/env python3
"""CPU-level cold-start and key-matrix regression; requires test-only py65.

Uses the built loader, stock ROMs and licensed local keyboard driver. Disk
calls are stubbed; keyboard scanning is real 6502 code against a CIA matrix
model. This complements (does not replace) the normal VICE round-trip suite.
"""
import argparse
import json
import re
from pathlib import Path
from py65.devices.mpu6502 import MPU
from verify_bridge_ui import ENV, ROOT


class Memory:
    def __init__(self, roms):
        self.ram = bytearray(65536)
        self.roms = roms
        self.keys = set()
        self.ram[0] = 0x2f
        self.ram[1] = 0x37

    def __getitem__(self, addr):
        if addr >= 0xe000 and self.ram[1] & 2:
            return self.roms['kernal'][addr-0xe000]
        if 0xa000 <= addr < 0xc000 and self.ram[1] & 3 == 3:
            return self.roms['basic'][addr-0xa000]
        if addr == 0xdc01:
            value = 255
            for row, col in self.keys:
                if not self.ram[0xdc00] & (1 << row):
                    value &= ~(1 << col)
            return value
        return self.ram[addr]

    def __setitem__(self, addr, value):
        self.ram[addr] = value


def invoke(cpu, address):
    cpu.stPushWord(0x02ef)
    cpu.pc = address
    for _ in range(30000):
        if cpu.pc == 0x02f0:
            return
        cpu.step()
    raise AssertionError(f'CPU call ${address:04x} failed at ${cpu.pc:04x}')


def cold(roms, pal, ide=True):
    mem = Memory(roms)
    mem.ram[0x02a6] = pal
    mem.ram[0x0706] = 0x23 if ide else 0
    mem.ram[0x0730:0x07a5] = bytes(117)
    mem.ram[0x0730:0x0734] = bytes([12 if ide else 8, 3 if ide else 0, 6 if ide else 0, 6])
    mem.ram[0x0734:0x073a] = b'BOOTER'
    mem.ram[0x0745:0x0748] = b'CP1'
    mem.ram[0x0765:0x076b] = b'CD//OS'
    mem.ram[0x0400:0x0700] = bytes([0xa5])*0x300
    vectors = bytes(range(26))
    mem.ram[0x031a:0x0334] = vectors
    code = (ROOT/'obj/c64os_cold_boot.bin').read_bytes()
    mem.ram[0xcf00:0xd000] = code
    cpu = MPU(memory=mem, pc=0xcf00)
    devices = []
    opens = 0
    # SETNAM/SETLFS execute in ROM; external OPEN/CLOSE/CLRCHN/CLALL are stubbed.
    for _ in range(2000000):
        if cpu.pc == 0xffd5:
            break
        if cpu.pc == 0xffba:
            devices.append(cpu.x)
        if cpu.pc == 0xffc0:
            opens += 1
        if cpu.pc in (0xffc0, 0xffc3, 0xffcc, 0xffe7):
            cpu.p &= ~1
            cpu.pc = (cpu.stPopWord()+1) & 65535
        else:
            cpu.step()
    else:
        raise AssertionError(f'Cold loader failed at ${cpu.pc:04x}')
    assert mem.ram[0x02a6] == pal
    assert mem.ram[0x0289] == 10
    assert mem.ram[0x028b:0x028d] == bytes([4, 10])
    assert mem.ram[0x028f:0x0291] == bytes.fromhex('48eb')
    assert mem.ram[0xdc04:0xdc06] == (bytes.fromhex('2540') if pal else bytes.fromhex('9542'))
    expected_vectors = vectors if ide else roms['kernal'][0x1d36:0x1d50]
    assert mem.ram[0x031a:0x0334] == expected_vectors
    assert devices == ([12, 12, 12] if ide else [8]), devices
    assert opens == (2 if ide else 0)
    assert mem.ram[0x0400:0x0700] == bytes([0xa5])*0x300
    # Model a key held when the cold loader enables the ordinary KERNAL IRQ.
    mem.keys = {(1, 2)}  # A
    invoke(cpu, 0xea87)
    assert mem.ram[0xc6] == 1 and mem.ram[0x277] == 0x41
    return mem, cpu


def detector_checks(roms):
    code = (ROOT/'bin/c64os.prg').read_bytes()
    start = int.from_bytes(code[:2], 'little')
    symbols = (ROOT/'obj/c64os.map').read_text()
    address = int(re.search(r'_bridge_detect_ide\s+([0-9A-F]+)', symbols)[1], 16)
    cases = [(b'IDE', 0x23, 0x23), (b'IDE', 0x3f, 0x3f),
             (b'IDE', 0x13, 255), (b'IDE', 0x83, 255), (b'IDE', 0x20, 255),
             (b'\xff\xff\xff', 0x23, 0), (b'IDx', 0x23, 0)]
    for signature, status, expected in cases:
        mem = Memory(roms)
        mem.ram[start:start+len(code)-2] = code[2:]
        mem.ram[0xde60:0xde63] = signature
        mem.ram[0xde32] = status
        before = bytes(mem.ram[0xde00:0xdf00])
        cpu = MPU(memory=mem)
        invoke(cpu, address)
        assert cpu.a == expected, (signature, status, cpu.a)
        assert mem.ram[1] == 0x37 and bytes(mem.ram[0xde00:0xdf00]) == before
    return len(cases)


def driver_checks(roms, repeat=4):
    mem, cpu = cold(roms, 1)
    driver = (ENV/'baseline/os/DRIVERS/kbd.c64.P00').read_bytes()[26:]
    addr = int.from_bytes(driver[:2], 'little')
    mem.ram[addr:addr+len(driver)-2] = driver[2:]
    mem.ram[0xc6] = 0
    mem.ram[0x0311] = 0
    mem.ram[0x03fd] = 0
    mem.ram[0x028b] = repeat
    mem.ram[0xc5] = 0x40
    mem.keys = set()
    def tick(keys):
        mem.keys = set(keys)
        invoke(cpu, addr)
        invoke(cpu, addr+3)
    tick(set())
    tick({(7, 4)})  # space: first key, then hold
    assert mem.ram[0xc6] == 1 and mem.ram[0x277] == 0x20
    mem.ram[0xc6] = 0
    for count in range(1, 301):
        tick({(7, 4)})
        if mem.ram[0xc6]:
            break
    else:
        raise AssertionError('Held space never repeated')
    if repeat == 0:
        assert count > 250  # Negative control reproduces the old missing default.
        return count
    assert count <= 21, count
    tick(set())
    mem.ram[0xc6] = 0
    tick({(1, 2)})
    assert mem.ram[0xc6] == 1 and mem.ram[0x277] == 0x41
    tick(set())
    mem.ram[0xc6] = 0
    tick({(1, 2), (1, 7)})  # shift-A
    assert mem.ram[0xc6] == 1 and mem.ram[0x277] == 0xc1
    tick(set())
    mem.ram[0xc6] = 0
    tick({(0, 0)})  # delete
    assert mem.ram[0xc6] == 1 and mem.ram[0x277] == 0x14
    tick(set())
    tick({(2, 5), (7, 2)})  # control-F command, separate native queue
    assert mem.ram[0x0311] == 1 and mem.ram[0x07fa] == 0x46
    return count


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom-dir', type=Path, default=Path('/opt/homebrew/share/vice/C64'))
    args = parser.parse_args()
    roms = {k: (args.rom_dir/f).read_bytes() for k, f in
            [('kernal', 'kernal-901227-03.bin'), ('basic', 'basic-901226-01.bin')]}
    for pal in (0, 1):
        for ide in (False, True):
            cold(roms, pal, ide)
    result = {'pass': True, 'scope': '6502 CPU and CIA key-matrix model; disk calls stubbed',
              'pal_and_ntsc_cold_start': True, 'held_key_before_booter': True,
              'ide64_and_plain_iec_boot_paths': True,
              'read_only_detection_cases': detector_checks(roms),
              'printable_shift_delete_command': True,
              'fixed_repeat_scans': driver_checks(roms),
              'old_zero_repeat_scans': driver_checks(roms, repeat=0)}
    (ENV/'startup-state.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
