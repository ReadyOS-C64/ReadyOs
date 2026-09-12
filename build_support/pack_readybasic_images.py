#!/usr/bin/env python3
"""Lossless RKC1 image packaging for MCFILE; ordinary Koala assets stay intact.

RKC1 + PackBits-like byte packets expands to exactly 10001 bytes in Koala
bitmap/screen/color/background order. Control low 7 bits + 1 is the count;
bit 7 selects a repeated following byte, otherwise count literal bytes follow.
There is no end marker: exact expanded length and physical EOF are required.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def pack(data):
    result = bytearray(b'RKC1')
    at = 0
    while at < len(data):
        end = at+1
        while end < len(data) and data[end] == data[at] and end-at < 128:
            end += 1
        if end-at >= 3:
            result.extend((128+end-at-1, data[at]))
            at = end
        else:
            end = at
            while end < len(data) and end-at < 128:
                if end+2 < len(data) and data[end] == data[end+1] == data[end+2]:
                    break
                end += 1
            result.append(end-at-1)
            result.extend(data[at:end])
            at = end
    return bytes(result)

def unpack(data):
    if data[:4] != b'RKC1':
        raise ValueError('bad RKC1 header')
    result = bytearray()
    at = 4
    while at < len(data):
        control = data[at]
        at += 1
        count = (control & 127)+1
        size = 1 if control & 128 else count
        if at+size > len(data):
            raise ValueError('truncated packet')
        result.extend(data[at:at+1]*count if control & 128 else data[at:at+count])
        at += size
        if len(result) > 10001:
            raise ValueError('oversized image')
    if len(result) != 10001:
        raise ValueError('short image')
    return bytes(result)

def main():
    out = ROOT/'obj/readybasic_images'
    out.mkdir(parents=True, exist_ok=True)
    for name, source in [('neon', 'neon/rb.neon.koa'),
                         ('warp', 'warped-city/rb.warp.koa')]:
        original = (ROOT/'assets/readybasic'/source).read_bytes()
        assert len(original) == 10003 and original[:2] == b'\0\x60'
        encoded = pack(original[2:])
        assert unpack(encoded) == original[2:]
        (out/f'rb.{name}.rkc').write_bytes(encoded)
        print(f'{name}: 10003 -> {len(encoded)} bytes, exact round-trip verified')

if __name__ == '__main__':
    main()
