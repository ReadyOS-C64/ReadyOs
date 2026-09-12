#!/usr/bin/env python3
"""Physical, disk-free USPEED/UMHZ regression at an already idle ReadyBASIC prompt.

Run from a Terminal-owned shell, after visually confirming boot completion.
No LOAD/RUN, reset, upload, VICE, or UCI transactions. REST memory access here
is permitted ONLY because the operator has confirmed there is no disk I/O.
The final configuration is C64U Turbo Registers / 1 MHz / badlines enabled.
"""
import argparse
import json
import os
from pathlib import Path
import re
import time
from urllib.parse import quote
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--idle-confirmed', action='store_true', required=True)
    args = parser.parse_args()
    assert args.idle_confirmed
    base = 'http://' + os.environ.get('C64U_HOST', '10.0.0.79') + '/v1/'
    folder = ROOT / 'build/readybasic-media-tools' / f'uspeed-{time.time_ns()}'
    folder.mkdir(parents=True)
    results = []

    def api(path, method='GET', payload=None):
        data = None if payload is None else json.dumps(payload).encode()
        with urlopen(Request(base+path, method=method, data=data,
                     headers={'Content-Type':'application/json'}), timeout=30) as response:
            return response.read()

    def checked(path, method='PUT', payload=None):
        value = json.loads(api(path, method, payload))
        assert not value.get('errors'), value
        return value

    def screen():
        data = api('machine:readmem?address=0400&length=1000')
        assert len(data) == 1000
        return '\n'.join(''.join(chr((v&127)+64) if (v&127)<32
                     else chr(v&127) if (v&127)<96 else ' '
                     for v in data[row:row+40]) for row in range(0,1000,40))

    def command(label, text, expected):
        # Clear the prior output to avoid accepting a stale marker. Each line
        # fits the C64 editor; no disk operations are allowed in this probe.
        text = 'PRINT CHR$(147):' + text
        assert len(text) <= 79, text
        data = (text+'\r').encode('ascii')
        for start in range(0,len(data),10):
            chunk = data[start:start+10]
            checked('machine:writemem?address=0277&data='+chunk.hex())
            checked('machine:writemem?address=00C6&data='+bytes([len(chunk)]).hex())
            time.sleep(.25)
        deadline = time.monotonic()+45
        while time.monotonic()<deadline:
            time.sleep(.3)
            view = screen()
            # Match a complete output row, never the echoed BASIC command.
            if re.search(expected, view, re.M):
                (folder/(label+'.txt')).write_text(view+'\n')
                results.append({'test':label,'expected':expected,'status':'passed'})
                print(label, 'PASS', flush=True)
                return view
        (folder/(label+'-failed.txt')).write_text(view+'\n')
        raise AssertionError(label+'\n'+view)

    def configure(mode):
        checked('configs','POST',{'U64 Specific Settings':{
            'Turbo Control':mode,'CPU Speed':' 1','Badline Timing':'Enabled'}})

    try:
        configure('C64U Turbo Registers')
        for mhz in (1,2,3,4,6,8,10,12,14,16,20,24,32,40,48,64):
            command(f'mhz-{mhz}', f'USPEED({mhz}):PRINT"LIVE";UMHZ()', rf'^LIVE {mhz} +$')
        preference = checked('configs/'+quote('U64 Specific Settings'), 'GET')
        (folder/'config-at-live-64.json').write_text(json.dumps(preference,indent=2)+'\n')
        command('assigned','S%=UMHZ():PRINT"ASSIGNED";S%',r'^ASSIGNED 64 +$')
        command('expression','PRINT"MATH";UMHZ()+1',r'^MATH 65 +$')
        command('invalid','USPEED(5)',r'^\?RB ERROR 14\s*$')
        command('invalid-atomic','PRINT"UNCHANGED";UMHZ()',r'^UNCHANGED 64 +$')
        command('badline-policy','POKE53297,128:USPEED(16):PRINT"BITS";PEEK(53297)',r'^BITS 137 +$')
        command('badline-query','PRINT"LIVE";UMHZ()',r'^LIVE 16 +$')
        command('restore-badlines','POKE53297,0:PRINT"LIVE";UMHZ()',r'^LIVE 1 +$')
        command('rom-integer','I%=0:REPEAT:I%=I%+1:UNTIL I%=3:PRINT"INT";I%',r'^INT 3 +$')
        for mhz in (1,16,64):
            view = command(f'bench-{mhz}',
                f'USPEED({mhz}):T=TI:FOR I=1 TO 1000:NEXT:PRINT"TICKS";TI-T',r'^TICKS \d+ +$')
            results[-1]['ticks'] = int(re.search(r'^TICKS (\d+)',view,re.M)[1])
        for mode in ('Off','Manual'):
            configure(mode)
            command('query-'+mode,'PRINT UMHZ()',r'^\?RB ERROR 24\s*$')
            command('setter-'+mode,'USPEED(16)',r'^\?RB ERROR 24\s*$')
    finally:
        configure('C64U Turbo Registers')
        (folder/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    command('final','PRINT"LIVE";UMHZ()',r'^LIVE 1 +$')
    (folder/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    ticks={r['test']:r['ticks'] for r in results if 'ticks' in r}
    assert ticks['bench-1']>4*ticks['bench-16'],ticks
    assert ticks['bench-16']>ticks['bench-64'],ticks
    print('USPEED/UMHZ PHYSICAL PROOF:',folder,flush=True)

if __name__=='__main__':
    main()
