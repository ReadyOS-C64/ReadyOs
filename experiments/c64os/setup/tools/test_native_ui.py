#!/usr/bin/env python3
"""Install the staged root BRIDGE.CAR through native Installer and test setup.
Run only against the owned bridge-setup-test emulator after host staging.
"""
import json,socket,sys,time,os,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent))
from verify_bridge_ui import Monitor,feed,wait_screen,screen
from verify_roundtrip_ui import mouse,native_command
PORT=6514
OUT=ROOT.parents[2].parent/'c64os-readyos-experiment/bridge-setup-test-20260926'
def click(m,x,y,double=False):
 end=time.monotonic()+5
 while m.read(0x310,1)!=b'\0':
  m.command(0xaa);time.sleep(.05)
  if time.monotonic()>end:raise AssertionError('Mouse queue did not drain')
 mouse(m,x,y,[1,3,4,1,3,5] if double else [1,3,4]);time.sleep(.15)
def choose(m,banks):
 for _ in range(26):
  t=screen(m);m.command(0xaa)
  match=re.search(r'(\d+) BANKS /',t.splitlines()[7])
  if match and int(match[1])==banks:return
  click(m,112,60)
 raise AssertionError('selector did not reach '+str(banks))
def go_apps(m):
 mouse(m,166,4,[1]);time.sleep(.1);mouse(m,192,52,[2,3]);return wait_screen(m,'BRIDGE SETUP')
def open_setup(m):
 t=go_apps(m);r=next(i for i,l in enumerate(t.splitlines()) if 'BRIDGE SETUP' in l)
 click(m,170,r*8+4,True);return wait_screen(m,'READY. CHOOSE APPLY OR UNDO.')
def boot(m):
 wait_screen(m,'COMMODORE 64 BASIC V2');feed(m,b'OPEN15,12,15,"CD//OS":CLOSE15\r');time.sleep(.3)
 feed(m,b'LOAD"BOOTER",12\r');time.sleep(1);feed(m,b'RUN\r');wait_screen(m,'DEVICES',60);wait_screen(m,'OFFLINE 12:',60)
def quit_setup(m):
 native_command(m,b'Q',4);wait_screen(m,'DEVICES')
with socket.create_connection(('127.0.0.1',PORT),5) as sock:
 sock.settimeout(20);m=Monitor(sock)
 rn=b'IDE64Image1';r=m.command(0x51,bytes([len(rn)])+rn)
 assert Path(r[2:2+r[1]].rstrip(b'\0').decode()).resolve()==(OUT/'runtime/vice support/c64os v1.09.hdd').resolve()
 try:
  print('starting native install',flush=True)
  wait_screen(m,'DEVICES');mouse(m,166,4,[1]);time.sleep(.1);mouse(m,192,20,[2,3])
  for _ in range(100):
   t=screen(m);m.command(0xaa);rows=[i for i,l in enumerate(t.splitlines()) if 'BRIDGE.CAR' in l[12:]]
   if rows:break
   time.sleep(.1)
  row=rows[0]
  click(m,168,row*8+4,True);wait_screen(m,'TYPE: INSTALL V3');click(m,175,156)
  boot(m);print('installed and rebooted',flush=True);t=open_setup(m);print('setup opened',flush=True);(OUT/'setup-initial.txt').write_text(t)
  live_banks=m.read(0x281,1);m.command(0xaa)
  choose(m,8);print('selected 8',flush=True)
  click(m,75,100);wait_screen(m,'APPLY SELECTED LIMIT?')
  click(m,52,124);wait_screen(m,'VERIFIED. RESTART C64OS NOW.')
  t=wait_screen(m,'ON DISK: 8 BANKS');(OUT/'setup-applied8.txt').write_text(t)
  assert m.read(0x281,1)==live_banks, 'Disk change must not resize a live allocator'
  m.command(0xaa)
  print('applied 8 and read back',flush=True)
  # Reopen to revalidate the installed cap, then update it to 9.
  quit_setup(m);open_setup(m);click(m,112,60);wait_screen(m,'9 BANKS / 0.5625 MB')
  click(m,75,100);wait_screen(m,'APPLY SELECTED LIMIT?');click(m,52,124)
  wait_screen(m,'VERIFIED. RESTART C64OS NOW.');wait_screen(m,'ON DISK: 9 BANKS')
  quit_setup(m);open_setup(m);click(m,223,100);wait_screen(m,'UNDO? NEXT BOOT MUST BE STANDALONE.')
  click(m,52,124);t=wait_screen(m,'VERIFIED. RESTART C64OS NOW.');assert 'ORIGINAL CAPACITY DETECTION' in t
  (OUT/'setup-undo.txt').write_text(t)
  # Reapply 32 on the same standalone session for the later bridge regression.
  quit_setup(m);open_setup(m)
  choose(m,32);wait_screen(m,'32 BANKS / 2.0000 MB');click(m,75,100);wait_screen(m,'APPLY SELECTED LIMIT?');click(m,52,124)
  t=wait_screen(m,'VERIFIED. RESTART C64OS NOW.');assert 'ON DISK: 32 BANKS' in t
  (OUT/'setup-restored32.txt').write_text(t)
  print(json.dumps({'pass':True,'native_installer':True,'apply':8,'update':9,'undo':True,'restored':32}))
 finally:m.command(0xaa)
