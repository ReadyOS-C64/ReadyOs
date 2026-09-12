#!/usr/bin/env python3
"""Terminal-owned live graphics comparison; run only after visually confirmed
LOAD of RBUGFXSNDDEMO at an idle ReadyBASIC prompt. RAM-only instrumentation
counts completed sprite batches and lines. Never saves changes to the disk.
The video-only readiness gate must succeed before any REST memory inspection.
Ends with Q cleanup and removes all temporary lines, leaving the demo loaded.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    gate = parser.add_mutually_exclusive_group(required=True)
    gate.add_argument('--loaded-idle-confirmed', action='store_true')
    gate.add_argument('--resume-live-confirmed', action='store_true',
        help='Resume after a measurement-only failure; same instrumented demo is visibly running, no disk I/O')
    parser.add_argument('--verify-setup', action='store_true',
        help='Check the updated demo\'s 64 MHz precalculation, 1 MHz disk stage and sequential line phase')
    args = parser.parse_args()
    folder = ROOT/'build/readybasic-media-tools'/f'graphics-speed-{time.time_ns()}'
    folder.mkdir(parents=True)
    host = os.environ.get('C64U_HOST', '10.0.0.79')

    def api(path, method='GET'):
        with urlopen(Request(f'http://{host}/v1/{path}', method=method), timeout=15) as response:
            value = response.read()
        if method == 'PUT':
            decoded = json.loads(value)
            assert not decoded.get('errors'), decoded
        return value

    def keys(value):
        data = value.encode('ascii')
        for offset in range(0, len(data), 10):
            chunk = data[offset:offset+10]
            api('machine:writemem?address=0277&data='+chunk.hex(), 'PUT')
            api('machine:writemem?address=00C6&data='+bytes([len(chunk)]).hex(), 'PUT')
            time.sleep(.3)

    def memory(address, length):
        return api(f'machine:readmem?address={address:04x}&length={length}')

    def counters():
        pointers = memory(0x2d, 4)
        first = int.from_bytes(pointers[:2], 'little')
        end = int.from_bytes(pointers[2:], 'little')
        assert 0x2ac1 < first < end < 0x9000
        raw = memory(first, end-first)
        result = {}
        for offset in range(0, len(raw), 7):
            value = raw[offset:offset+7]
            name = bytes(b & 127 for b in value[:2]).decode('ascii').rstrip('\0')
            if name not in ('AF', 'AL', 'MS', 'PS', 'LS', 'LP', 'RC', 'P', 'SP', 'LD'):
                continue
            if name in ('MS', 'PS', 'LS', 'LP', 'P', 'SP', 'LD'):
                result[name] = int.from_bytes(value[2:4], 'big', signed=True)
            elif value[2] == 0:
                result[name] = 0
            else:
                bits = int.from_bytes(value[3:], 'big')
                result[name] = (1+(bits & 0x7fffffff)/2**31)*2**(value[2]-129)
        assert {'AF', 'AL', 'MS'} <= set(result), result
        if args.verify_setup:
            assert result['PS'] == 64 and result['LS'] == 1, result
            # LP updates inside WEAVE, AL just after its return. Sampling can
            # catch that one-call boundary, but no larger phase gap is valid.
            count = int(result['AL'])
            assert result['LP'] in {count//2 & 255, (count+1)//2 & 255}, result
            frames = int(result['AF'])
            assert result['P'] in {frames*result['SP'] & 255,
                                   (frames+1)*result['SP'] & 255}, result
            assert result['LD'] == 3, result
            assert count in {frames//3, max(0, frames//3-1)}, result
        return result

    lines = {
        275: 'AF=0:AL=0:MS%=UMHZ()',
        335: 'AF=AF+1',
        365: 'AL=AL+1',
        392: 'IF A$="1" THEN :USPEED(1):MS%=UMHZ()',
        393: 'IF A$="6" THEN :USPEED(16):MS%=UMHZ()',
    }
    if args.verify_setup:
        lines.update({186: 'PS%=UMHZ()', 216: 'LS%=UMHZ()'})
    if not args.resume_live_confirmed:
        for line, body in lines.items():
            keys(f'{line} {body}\r')
        keys('RUN\r')
        subprocess.run(['/usr/local/share/dotnet/dotnet', 'run', '--project',
            str(ROOT/'build_support/readybasic_video_gate/Video.csproj'), '--', str(folder/'video')], check=True)
    # Complete backdrop plus moving letters confirms that disk loading ended.
    results = []
    for mhz in (16, 1, 16):
        keys('6' if mhz == 16 else '1')
        # At 1 MHz a graphics iteration can exceed two seconds (especially
        # during background restore). Wait for consumption, not a fixed delay.
        deadline = time.monotonic()+20
        while True:
            before = counters()
            if before['MS'] == mhz:
                break
            assert time.monotonic() < deadline, before
            time.sleep(.4)
        time.sleep(1)
        before = counters()
        started = time.monotonic()
        time.sleep(8)
        after = counters()
        elapsed = time.monotonic()-started
        assert after['MS'] == mhz, after
        result = dict(mhz=mhz, host_seconds=elapsed, before=before, after=after,
            sprite_batches_per_second=(after['AF']-before['AF'])/elapsed,
            lines_per_second=(after['AL']-before['AL'])/elapsed)
        results.append(result)
        (folder/'results.json').write_text(json.dumps(results, indent=2)+'\n')
        print(result, flush=True)
    if args.verify_setup:
        before = counters()
        tick = memory(0x9006, 2)
        keys(' ')
        deadline = time.monotonic()+5
        while True:
            after = counters()
            if after['RC'] > before['RC']:
                break
            assert time.monotonic() < deadline, (before, after)
            time.sleep(.3)
        assert 1 <= after['RC']-before['RC'] <= 2, (before, after)
        time.sleep(.3)
        assert memory(0xc1ff, 1) == b'\x02'
        assert memory(0x9006, 2) != tick, 'music ticks did not advance'
        (folder/'space-music.json').write_text(json.dumps(dict(before=before, after=after), indent=2)+'\n')
        print('SPACE RESTORE AND MUSIC LIVENESS PASSED', flush=True)
    # No program/disk writes: remove instrumentation after the normal exit.
    keys('Q')
    time.sleep(3)
    assert memory(0xc1ff, 1) == b'\x00', 'music cleanup incomplete'
    assert memory(0x37, 2) == b'\x00\xa0', 'memory cap cleanup incomplete'
    for line in lines:
        keys(f'{line}\r')
    keys('PRINT CHR$(147):PRINT"GRAPHICS TEST DONE":PRINT UMHZ()\r')
    print('GRAPHICS COMPARISON COMPLETE:', folder, flush=True)

if __name__ == '__main__':
    main()
