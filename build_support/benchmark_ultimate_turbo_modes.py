#!/usr/bin/env python3
"""Compare Ultimate control paths using host monotonic time, not CIA/TI/UMHZ.

Terminal-owned shell only. Requires a visually confirmed idle BASIC prompt;
performs no disk I/O. --readybasic uses USPEED rather than POKE for D031 cases.
Leaves C64U Turbo Registers mode, 1 MHz, badlines enabled. All measurements
include bounded REST polling overhead; compare control paths, not exact MHz.
"""
import argparse
import json
import os
from pathlib import Path
import re
import time
from urllib.request import Request, urlopen

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--idle-confirmed',action='store_true',required=True)
    p.add_argument('--readybasic',action='store_true')
    args=p.parse_args()
    base='http://'+os.environ.get('C64U_HOST','10.0.0.79')+'/v1/'
    folder=ROOT/'build/readybasic-media-tools'/f'turbo-wall-{time.time_ns()}'
    folder.mkdir(parents=True)
    results=[]

    def api(path,method='GET',payload=None):
        data=None if payload is None else json.dumps(payload).encode()
        with urlopen(Request(base+path,method=method,data=data,
                  headers={'Content-Type':'application/json'}),timeout=15) as response:
            return response.read()

    def checked(path,method='PUT',payload=None):
        result=json.loads(api(path,method,payload));assert not result.get('errors'),result

    def configure(mode,mhz):
        checked('configs','POST',{'U64 Specific Settings':{
            'Turbo Control':mode,'CPU Speed':f'{mhz:2d}','Badline Timing':'Enabled'}})

    def execute(text):
        assert len(text)<=79,text
        data=(text+'\r').encode('ascii')
        for start in range(0,len(data),10):
            chunk=data[start:start+10]
            checked('machine:writemem?address=0277&data='+chunk.hex())
            if start+10>=len(data):began=time.monotonic()
            checked('machine:writemem?address=00C6&data='+bytes([len(chunk)]).hex())
            time.sleep(.2)
        while time.monotonic()-began<90:
            # Only three rows, no disk activity. Whole-output-line match
            # excludes the echoed command. CHR$(147) removed earlier results.
            raw=api('machine:readmem?address=0400&length=120')
            text='\n'.join(''.join(chr((b&127)+64) if (b&127)<32 else chr(b&127)
                            for b in raw[row:row+40]) for row in (0,40,80))
            if re.search(r'^DONE\s*$',text,re.M):return time.monotonic()-began,text
            if '?' in text:raise AssertionError(text)
            time.sleep(.1)
        raise TimeoutError(text)

    cases=[('register-1','C64U Turbo Registers',1,1),
           ('register-16','C64U Turbo Registers',1,16),
           ('manual-16','Manual',16,None),
           ('gouraud-enabled-16','TurboEnable Bit',16,True),
           ('gouraud-disabled-1','TurboEnable Bit',16,False)]
    try:
        for label,mode,preference,setting in cases:
            configure(mode,preference)
            if mode=='C64U Turbo Registers':
                select=f'USPEED({setting})' if args.readybasic else f'POKE53297,{0 if setting==1 else 9}'
            elif mode=='TurboEnable Bit':select=f'POKE53296,{int(setting)}'
            else:select='REM'
            # Manual mode has no in-program selector; avoid REM eating rest.
            select='' if select=='REM' else select+':'
            command='PRINT CHR$(147):'+select+'FOR I=1 TO 10000:NEXT:PRINT"DONE"'
            elapsed,screen=execute(command)
            result={'case':label,'mode':mode,'menu_mhz':preference,
                    'command':command,'host_seconds':round(elapsed,4)}
            results.append(result)
            (folder/(label+'.txt')).write_text(screen+'\n')
            (folder/'results.json').write_text(json.dumps(results,indent=2)+'\n')
            print(result,flush=True)
    finally:
        configure('C64U Turbo Registers',1)
    times={r['case']:r['host_seconds'] for r in results}
    assert times['register-1']>5*times['register-16'],times
    assert .65<times['register-16']/times['manual-16']<1.5,times
    assert .65<times['register-16']/times['gouraud-enabled-16']<1.5,times
    assert .65<times['register-1']/times['gouraud-disabled-1']<1.5,times
    print('WALL-CLOCK CONTROL-PATH COMPARISON PASSED:',folder,flush=True)

if __name__=='__main__':main()
