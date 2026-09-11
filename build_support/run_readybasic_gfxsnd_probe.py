#!/usr/bin/env python3
"""Both exits of the readable standard demo, booted through ReadyOS in VICE."""
import json
import os
import re
from pathlib import Path
import subprocess
import time

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/readybasic-media-tools'
subprocess.run(['python3',str(ROOT/'build_support/run_readybasic_media_probe.py')],
    env={**os.environ,'READYBASIC_GENERATE_PLAN_ONLY':'1'},check=True)
p=json.loads((OUT/'probe.yaml').read_text())
steps=p['steps'][:6]
steps[4]['params'].update(pre_delay_s=15,poll_s=1)
tag=str(time.time_ns())
def add(kind,name,**params):steps.append(dict(type=kind,id=name,params=params))
def keys(name,text,delay=1):
    add('input.sequence',name,keys=list(text.encode()),inter_key_delay_s=.03,post_delay_s=delay)
def screen(name,text,delay=0):
    add('screen.wait_contains',name,text=text,pre_delay_s=delay,wait_timeout_s=180,capture_label=name)
def mem(name,addr,data):add('assert.memory',name,start=addr,end=addr+len(data)-1,equals_hex=data.hex(' '))
def capture(name):
    add('screen.capture',name,label=name)
    add('dump.memory_ranges',name+'_state',ranges=[
        dict(label='vic',start=0xd000,end=0xd02e),
        dict(label='screen',start=0xcc00,end=0xcfe7),
        dict(label='color',start=0xd800,end=0xdbe7),
        dict(label='ticks',start=0x9006,end=0x9007)])
keys('unsupported_speed','USPEED(1)\r')
screen('unsupported_error','?RB ERROR')
keys('invalid_speed','USPEED(5)\r')
screen('invalid_error','?RB ERROR')
keys('set_colors','BORDER(4):MCBG(6):POKE646,14\r')
keys('load_demo','LOAD "RBGFXSNDDEMO",8\r',3)
add('monitor.command','realtime',command='warp off')
keys('run_demo','RUN\r',.2)
screen('started','ORBITAL SHOW RUNNING',100)
mem('playing',0xc1ff,b'\x02')
add('assert.screen_not_contains','no_startup_error',not_contains='?')
capture('show_a')
screen('motion_wait','ORBITAL SHOW RUNNING',1)
capture('show_b')
def refresh_sample(name):
    add('dump.memory_ranges',name,ranges=[
        dict(label='pointers',start=0x2d,end=0x30),
        dict(label='workspace',start=0x2ac1,end=0x8fff)])
refresh_sample('before_space')
keys('space_refresh',' ',.3)
refresh_sample('after_space')
mem('space_keeps_music',0xc1ff,b'\x02')
screen('refresh_wait','ORBITAL SHOW RUNNING',17)
keys('music_exit','M',2)
screen('music_continues','MUSIC CONTINUES')
mem('music_retained',0xc1ff,b'\x02')
mem('memory_retained',0x37,b'\x00\x90')
keys('music_colors','PRINT "COLORS";(PEEK(53280)AND15);(PEEK(53281)AND15);PEEK(646)\r')
screen('music_colors_ok','COLORS 4  6  14')
add('dump.memory_ranges','music_prompt_a',ranges=[dict(label='ticks',start=0x9006,end=0x9007)])
screen('prompt_wait','MUSIC CONTINUES',2)
add('dump.memory_ranges','music_prompt_b',ranges=[dict(label='ticks',start=0x9006,end=0x9007)])
# Re-running with music still active must drop it before touching the disk.
keys('rerun','RUN\r',.2)
screen('restarted','ORBITAL SHOW RUNNING',100)
mem('playing_again',0xc1ff,b'\x02')
keys('full_exit','Q',2)
screen('all_stopped','ALL STOPPED')
mem('music_released',0xc1ff,b'\x00')
mem('memory_released',0x37,b'\x00\xa0')
# A held/repeated exit key can spill into the newly idle BASIC editor.
# Clear that input line before submitting the diagnostic expression.
keys('clear_exit_line','\x14'*8)
keys('full_colors','PRINT "COLORS";(PEEK(53280)AND15);(PEEK(53281)AND15);PEEK(646)\r')
screen('full_colors_ok','COLORS 4  6  14')
p.update(plan_id='readybasic_gfxsnd_'+tag,steps=steps)
path=OUT/f'gfxsnd-{tag}.yaml'
path.write_text(json.dumps(p,indent=2)+'\n')
if os.environ.get('READYBASIC_GENERATE_PLAN_ONLY')=='1':
    print(path);raise SystemExit(0)
project=ROOT.parent/'agenticdevharness/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj'
subprocess.run(['dotnet','run','--project',str(project),'--','run','--plan',str(path),'--close-vice'],check=True,cwd=ROOT)
manifest=next(m for m in (ROOT/'logs').glob('vice_auto_*/manifest.json')
    if m.parent.name!='vice_auto_latest' and json.loads(m.read_text(encoding='utf-8-sig'))['plan_id']==p['plan_id'])
st=manifest.parent/'stages'
def refresh_count(name):
    stage=st/name
    pointers=(stage/'pointers.bin').read_bytes()
    start=int.from_bytes(pointers[:2],'little')
    end=int.from_bytes(pointers[2:],'little')
    workspace=(stage/'workspace.bin').read_bytes()
    for address in range(start,end,7):
        value=workspace[address-0x2ac1:address-0x2ac1+7]
        if value[:2]==b'RC':
            exponent=value[2]
            if not exponent:return 0
            mantissa=int.from_bytes(value[3:],'big')
            assert not mantissa & 0x80000000
            return (1+(mantissa&0x7fffffff)/2**31)*2**(exponent-129)
    raise AssertionError('refresh counter RC missing')
# The automatic timer may expire between the samples as well as the keypress.
# Both resets are valid; two automatic resets cannot occur in this short window.
delta=refresh_count('after_space')-refresh_count('before_space')
assert delta in (1,2), f'Space refresh count changed unexpectedly: {delta}'
assert (st/'show_a_state/vic.bin').read_bytes()[:10]!=(st/'show_b_state/vic.bin').read_bytes()[:10]
assert (st/'music_prompt_a/ticks.bin').read_bytes()!=(st/'music_prompt_b/ticks.bin').read_bytes()
exit_text=(manifest.parent/'screen_decoded/music_continues.txt').read_text(encoding='utf-8-sig')
refresh=re.search(r'IMAGE REFRESHES:\s+(\d+)',exit_text)
assert refresh and int(refresh[1])>0, 'no timed image refresh observed'
koa=(ROOT/'assets/readybasic/neon/rb.neon.koa').read_bytes()
for sample in ('show_a','show_b'):
    assert (st/(sample+'_state')/'screen.bin').read_bytes()==koa[8002:9002]
    assert bytes(x&15 for x in (st/(sample+'_state')/'color.bin').read_bytes())==koa[9002:10002]
print('GFXSND VERIFIED: both exits, text colors, live prompt music, safe rerun, palette and movement.',manifest)
