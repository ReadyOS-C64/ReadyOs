"""Preload ReadyOS demo apps on a fresh boot; never start C64OS here."""
import json,os,socket,sys
from pathlib import Path
from verify_bridge_ui import Monitor,feed,wait_screen
from verify_roundtrip_ui import launcher_app
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(os.environ.get('READYOS_DEMO_OUT',str(ROOT.parent/'c64os-readyos-experiment/bridge-demo-20260925')))
OUT.mkdir(parents=True,exist_ok=True)
with socket.create_connection(('127.0.0.1',6511),5) as s:
 s.settimeout(20);m=Monitor(s)
 launcher_app(m,1);wait_screen(m,'EDITOR:');feed(m,[2]);wait_screen(m,'APPLICATIONS')
 launcher_app(m,8);wait_screen(m,'REU MEMORY MAP');feed(m,[2]);wait_screen(m,'APPLICATIONS')
 launcher_app(m,0);lines=wait_screen(m,'START / RESUME C64OS (C)').splitlines()
 for row,text in [(1,'READYOS / C64OS'),(10,'SWITCH BACK USING THE READYOS UTILITY'),(11,'IN THE C64OS MENU.'),(22,'UP/DOWN:SELECT  RETURN:OPEN'),(23,'F2:NEXT APP  F4:PREV APP  CTRL+B:HOME')]:
  assert lines[row].index(text)==(40-len(text))//2,(row,lines[row])
 assert lines[24].strip()==''
 feed(m,[2]);wait_screen(m,'APPLICATIONS')
 for name,value in [('Drive8TrueEmulation',0),('TrapDevice8',1)]:
  n=name.encode();m.command(0x52,bytes([1,len(n)])+n+bytes([4])+value.to_bytes(4,'little'))
  got=m.command(0x51,bytes([len(n)])+n)
  assert int.from_bytes(got[2:],'little')==value,(name,got)
 m.command(0xaa)
(OUT/'prepare.json').write_text(json.dumps({'centered_rows':[1,10,11,22,23],'initial_status_row_blank':True,'editor_and_reuviewer_preloaded':True,'c64os_started':False,'drive8_true_emulation':False,'drive8_traps':True},indent=2)+'\n')
print('ReadyOS apps preloaded; C64OS not started; traps enabled',flush=True)
