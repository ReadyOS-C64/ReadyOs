#!/usr/bin/env python3
"""Terminal-owned physical proof; prepare the exact-path D81 first.

Enables software-controlled turbo, uploads only a fresh owned test image, and
boots ReadyOS. Confirm boot and BASIC-file loading on the physical display;
resource-loading RUNs use the video-only gate. Leaves the Ultimate demo running
after proving both exits and a music-active rerun.
"""
import json
import os
import re
from pathlib import Path
import subprocess
import time
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/readybasic-media-tools'
deployment = json.loads((OUT/'neon-deployment.json').read_text())
host = os.environ.get('C64U_HOST', '10.0.0.79')
request = Request(f'http://{host}/v1/configs', method='POST',
    headers={'Content-Type': 'application/json'}, data=json.dumps({
    'U64 Specific Settings': {'Turbo Control': 'C64U Turbo Registers', 'CPU Speed': ' 1'}}).encode())
with urlopen(request, timeout=30) as response:
    result = json.load(response)
    assert not result.get('errors'), result
subprocess.run(['python3', str(ROOT/'build_support/deploy_readybasic_neon_ultimate.py')], check=True)
# Boot timing is not completion evidence. Do not inspect RAM to discover
# whether the disk has finished: that inspection can itself hang C64U I/O.
input('Watch the physical display. When ReadyBASIC has finished booting and '
      'is idle at READY., press Enter (do not acknowledge while loading): ')
p = json.loads(Path(deployment['plan']).read_text())
steps = p['steps'][:2]
def add(kind, name, **params):
    steps.append(dict(type=kind, id=name, params=params))
def keys(name, text, delay=1):
    assert all(len(line)<=79 for line in text.split('\r')), 'C64 input line too long'
    add('input.sequence', name, keys=list(text.encode()), inter_key_delay_s=.05, post_delay_s=delay)
def screen(name, text, delay=0):
    add('screen.wait_contains', name, text=text, pre_delay_s=delay,
        wait_timeout_s=90, capture_label=name)
def memory(name, start, data):
    add('assert.memory', name, start=start, end=start+len(data)-1, equals_hex=data.hex(' '))
def capture(name):
    add('assert.screen_not_contains', name+'_no_error', not_contains='?')
    add('screen.capture', name, label=name, capture_state=True)
    add('dump.memory_ranges', name+'_state', ranges=[
        dict(label='screen', start=0xcc00, end=0xcfe7),
        dict(label='color', start=0xd800, end=0xdbe7),
        dict(label='sprites', start=0xca00, end=0xcb3f),
        dict(label='ticks', start=0x9006, end=0x9007)])
keys('speed16', 'USPEED(16):PRINT "SPEED16";(PEEK(53297)AND15)\r')
screen('speed16_ok', 'SPEED16 9')
keys('speed64', 'USPEED(64):PRINT "SPEED64";(PEEK(53297)AND15)\r')
screen('speed64_ok', 'SPEED64 15')
keys('speed1', 'USPEED(1):PRINT "SPEED1";(PEEK(53297)AND15)\r')
screen('speed1_ok', 'SPEED1 0')
keys('colors', 'BORDER(4):MCBG(6):POKE646,14\r')
keys('load', 'LOAD "RBUGFXSNDDEMO",8\r', 20)
# Program is already loaded: prove its first command downshifts from turbo.
keys('turbo_before_run', 'USPEED(16)\r')
keys('run', 'RUN\r')  # Boundary marker only; replaced by run_then_video below.
screen('running', 'ORBITAL SHOW RUNNING')
memory('music_live', 0xc1ff, b'\x02')
capture('show_a')
screen('motion', 'ORBITAL SHOW RUNNING', 2)
capture('show_b')
screen('refresh', 'ORBITAL SHOW RUNNING', 17)
keys('music_exit', 'M', 3)
screen('music_continues', 'MUSIC CONTINUES')
memory('music_retained', 0xc1ff, b'\x02')
memory('memory_retained', 0x37, b'\x00\x90')
keys('check_m', 'PRINT "RESTORED";(PEEK(53280)AND15);(PEEK(53281)AND15);PEEK(646)\r')
screen('m_restored', 'RESTORED 4  6  14')
keys('check_m_speed', 'PRINT "SPEED";(PEEK(53297)AND15)\r')
screen('m_speed', 'SPEED 0')
add('dump.memory_ranges', 'prompt_a', ranges=[dict(label='ticks', start=0x9006, end=0x9007)])
screen('music_wait', 'MUSIC CONTINUES', 2)
add('dump.memory_ranges', 'prompt_b', ranges=[dict(label='ticks', start=0x9006, end=0x9007)])
keys('rerun_music_active', 'RUN\r')  # Boundary marker; never executed by the harness.
memory('rerun_live', 0xc1ff, b'\x02')
keys('full_exit', 'Q', 3)
screen('all_stopped', 'ALL STOPPED')
memory('music_released', 0xc1ff, b'\0')
memory('memory_released', 0x37, b'\x00\xa0')
keys('check_q', 'PRINT "RESTORED";(PEEK(53280)AND15);(PEEK(53281)AND15);PEEK(646)\r')
screen('q_restored', 'RESTORED 4  6  14')
keys('check_q_speed', 'PRINT "SPEED";(PEEK(53297)AND15)\r')
screen('q_speed', 'SPEED 0')
keys('handoff_text_colors', 'BORDER(6):MCBG(6):POKE646,1\r')
keys('leave_running', 'RUN\r')  # Boundary marker; never executed by the harness.
memory('handoff_music', 0xc1ff, b'\x02')
capture('handoff')
tag = str(time.time_ns())
project = ROOT.parent/'agenticdevharness/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj'
def block(label, selected):
    plan={**p, 'plan_id':f'readybasic_gfxsnd_ultimate_{tag}_{label}', 'steps':[steps[0]]+selected}
    path=OUT/f'gfxsnd-ultimate-{tag}-{label}.yaml'
    path.write_text(json.dumps(plan, indent=2)+'\n')
    print('Safe inspection block:', label, flush=True)
    subprocess.run(['dotnet', 'run', '--project', str(project), '--', 'run-ultimate-plan',
        '--plan', str(path), '--no-tui'], check=True, cwd=ROOT,
        env={**os.environ, 'ULTIMATE_ASSUME_MOUNTED':'1'})
    return next(m for m in (ROOT/'logs').glob('ultimate_auto_*/manifest.json')
        if m.parent.name!='ultimate_auto_latest' and
        json.loads(m.read_text(encoding='utf-8-sig'))['plan_id']==plan['plan_id'])

def run_then_video(label):
    # We are at an idle BASIC prompt. The final count write starts RUN;
    # absolutely no subsequent memory access until the video gate passes.
    for query in ('address=00C5&data=0000', 'address=0277&data=52554e0d', 'address=00C6&data=04'):
        with urlopen(Request(f'http://{host}/v1/machine:writemem?{query}', method='PUT'),timeout=30) as response:
            assert not json.load(response).get('errors')
    print('Watching video only:', label, flush=True)
    subprocess.run(['dotnet','run','--project',str(ROOT/'build_support/readybasic_video_gate/Video.csproj'),
        '--',str(OUT/f'gfxsnd-video-{tag}-{label}')],check=True,cwd=ROOT)

start=next(i for i,s in enumerate(steps) if s['id']=='run')
rerun=next(i for i,s in enumerate(steps) if s['id']=='rerun_music_active')
handoff=next(i for i,s in enumerate(steps) if s['id']=='leave_running')
load=next(i for i,s in enumerate(steps) if s['id']=='load')
prelude=block('prelude',steps[1:load+1])
input('Watch the physical display. When LOAD has completed and READY. has '
      'returned, press Enter (do not acknowledge while loading): ')
loaded=block('loaded',steps[load+1:start])
run_then_video('first')
manifest=block('music_exit',steps[start+1:rerun])
run_then_video('rerun')
second=block('full_exit',steps[rerun+1:handoff])
run_then_video('handoff')
final=block('handoff',steps[handoff+1:])
st=manifest.parent/'stages'
assert (st/'prompt_a/ticks.bin').read_bytes() != (st/'prompt_b/ticks.bin').read_bytes()
exit_text=(manifest.parent/'screen_decoded/music_continues.txt').read_text(encoding='utf-8-sig')
refresh=re.search(r'IMAGE REFRESHES:\s+(\d+)',exit_text)
assert refresh and int(refresh[1])>0, 'no timed image refresh observed'
pictures = [(ROOT/'assets/readybasic'/name).read_bytes() for name in
    ('neon/rb.neon.koa', 'warped-city/rb.warp.koa')]
spr = (ROOT/'assets/readybasic/neon/rb.ready.rbr').read_bytes()[8:]
for sample in ('show_a', 'show_b', 'handoff'):
    base=final.parent/'stages' if sample=='handoff' else st
    screen = (base/(sample+'_state')/'screen.bin').read_bytes()
    color = bytes(x&15 for x in (base/(sample+'_state')/'color.bin').read_bytes())
    assert any(screen==koa[8002:9002] and color==koa[9002:10002] for koa in pictures)
    assert (base/(sample+'_state')/'sprites.bin').read_bytes()==spr
reports=[str(m) for m in (prelude,loaded,manifest,second,final)]
(OUT/'gfxsnd-ultimate-verified.json').write_text(json.dumps(reports,indent=2)+'\n')
print('ULTIMATE GFXSND VERIFIED:', *reports, sep='\n')
