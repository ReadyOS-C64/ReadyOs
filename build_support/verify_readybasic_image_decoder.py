#!/usr/bin/env python3
"""Exercise the built 6502 resource decoder with a simulated KERNAL byte stream.

Requires test-only py65. This checks decoding/bounds/cleanup, not IEC timing.
Build the normal ReadyOS profile first; this script never builds an app.
"""
from pathlib import Path
from py65.devices.mpu6502 import MPU
from pack_readybasic_images import pack, unpack

ROOT = Path(__file__).resolve().parents[1]
LABELS = {line.split()[2].lstrip('.'): int(line.split()[1], 16)
          for line in (ROOT/'obj/readybasic_modules/media.labels').read_text().splitlines()}
MODULE = (ROOT/'obj/readybasic_modules/media.bin').read_bytes()

def run(data, expected=None, entry='koaload'):
    memory = [0xA5]*65536
    memory[0xB000:0xB000+len(MODULE)] = MODULE
    memory[0xC1FF] = 0
    memory[0xC250] = 4
    memory[0xC260:0xC264] = b'test'
    memory[0x98] = 0
    memory[0xD015] = 31
    memory[0xD011] = 59
    address = LABELS[entry]
    memory[0x200:0x204] = [0x20, address & 255, address >> 8, 0]
    cpu = MPU(memory=memory, pc=0x200)
    position = status = 0
    traps = {0xFFBD,0xFFBA,0xFFC0,0xFFC6,0xFFCF,0xFFB7,0xFFCC,0xFFC3}
    for steps in range(2000000):
        if cpu.pc == 0x203:
            break
        if cpu.pc in traps:
            if cpu.pc == 0xFFCF:
                assert position < len(data), 'read beyond EOF'
                cpu.a = data[position]
                position += 1
                status = 64 if position == len(data) else 0
            elif cpu.pc == 0xFFB7:
                cpu.a = status
            if cpu.pc in (0xFFCF,0xFFB7):
                cpu.p = (cpu.p & ~0x82) | (0x02 if cpu.a == 0 else 0) | (cpu.a & 0x80)
            cpu.p &= ~1
            cpu.pc = (cpu.stPopWord()+1) & 65535
        else:
            cpu.step()
    else:
        raise AssertionError('decoder did not terminate')
    assert memory[0xD015] == 31 and memory[0xD011] == 59, 'display cleanup'
    assert memory[0xC600:0xCA00] == [0xA5]*1024, 'shim guard'
    assert memory[0xDFE0:0xE000] == [0xA5]*32, 'bitmap lower guard'
    assert memory[0xFF40:0xFF60] == [0xA5]*32, 'bitmap upper guard'
    assert memory[0xCFE8:0xD000] == [0xA5]*24, 'screen tail guard'
    assert memory[0xDBE8:0xDC00] == [0xA5]*24, 'color tail guard'
    if expected is None:
        assert memory[0xC300] in (21,22), ('expected rejection',memory[0xC300])
    else:
        assert memory[0xC300] == 0, ('unexpected failure',memory[0xC300])
        assert position == len(data)
        if entry == 'koaload':
            actual = bytes(memory[0xE000:0xFF40]+memory[0xCC00:0xCFE8]+
                           memory[0xD800:0xDBE8]+[memory[0xD021]])
            assert actual == expected[:-1]+bytes([expected[-1]&15])
            assert memory[0xCA00:0xCC00] == [0xA5]*512, 'sprite guard'
        else:
            assert bytes(memory[0xCA00:0xCB40]) == expected

def main():
    tests = 0
    for source in ('neon/rb.neon.koa','warped-city/rb.warp.koa'):
        data = (ROOT/'assets/readybasic'/source).read_bytes()
        encoded = pack(data[2:])
        assert unpack(encoded) == data[2:]
        run(data, data[2:]); run(encoded, data[2:]); tests += 2
        for bad in (encoded[:-1],encoded+b'\0',b'RKC2'+encoded[4:],data[:-1],data+b'\0'):
            run(bad); tests += 1
    for payload in (bytes(10001),bytes(i%256 for i in range(10001)), bytes([255])*10001):
        run(pack(payload),payload); tests += 1
    # Last repeated packet crosses the hard expanded-size boundary.
    run(pack(bytes(10002))); tests += 1
    run(b'RKC1\x7f\x01'); tests += 1
    sprite = (ROOT/'assets/readybasic/neon/rb.ready.rbr').read_bytes()
    run(sprite,sprite[8:],'sprfile'); tests += 1
    print(f'6502 DECODER PASS: {tests} cases; Koala/RKC1/sprites, malformed input, bounds and display cleanup')

if __name__ == '__main__':
    main()
