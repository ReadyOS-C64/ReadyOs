#!/usr/bin/env python3
"""From a fresh launcher, verify bridge TUI controls and preload demo apps."""
import sys,socket,json,time
from pathlib import Path
from verify_bridge_ui import Monitor,feed,wait_screen,screen,screenshot,ENV
from verify_roundtrip_ui import launcher_app,native_command
with socket.create_connection(('127.0.0.1',6511),5) as s:
 s.settimeout(15);m=Monitor(s)
 launcher_app(m,1);wait_screen(m,'EDITOR:');feed(m,[2]);wait_screen(m,'APPLICATIONS')
 launcher_app(m,8);print(wait_screen(m,'REU MEMORY MAP'));feed(m,[2]);wait_screen(m,'APPLICATIONS')
 launcher_app(m,0);wait_screen(m,'START / RESUME C64 OS (C)')
 feed(m,[19,17]);t=wait_screen(m,'> TEST RAM SAVE / RESTORE (S)');feed(m,[13]);wait_screen(m,'RESTORED ZP/STACK/SHIM: 1')
 feed(m,[17,13]);wait_screen(m,'COLD BOOT FAILED: 4; READYOS RESTORED')
 feed(m,b'C');wait_screen(m,'DEVICES',timeout=60);wait_screen(m,'OFFLINE');native_command(m);wait_screen(m,'RESTORED ZP/STACK/SHIM: 2')
 screenshot('bridge-tui-menu.png')
 tokens=[]
 for key in (137,137,137,138,138,138):
  feed(m,[key]);time.sleep(.35);t=screen(m);tokens.append(m.read(0xc834,1)[0]);m.command(0xaa);print(t.splitlines()[0])
 assert len(set(tokens))==3,tokens
 wait_screen(m,'C64 OS BRIDGE');feed(m,[2]);wait_screen(m,'APPLICATIONS')
report={'pass':True,'menu_return_ram_proof':True,'menu_return_error_recovery':True,'shortcut_cold_round_trip':True,'f2_f4_app_tokens':tokens,'ctrl_b':True,'editor_reuviewer_preloaded':True}
(ENV/'bridge-tui-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
