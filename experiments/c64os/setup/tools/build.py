#!/usr/bin/env python3
"""Build native Bridge Setup and one install CAR. No licensed OS bytes packaged."""
import binascii, datetime, hashlib, json, subprocess, argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def petscii(s):
 return bytes(13 if c=='\n' else ord(c.upper()) if 'a'<=c<='z' else ord(c)|128 if 'A'<=c<='Z' else ord(c) for c in s)
def asm(s): return '.byte '+','.join(str(x) for x in petscii(s)+b'\0')+'\n'
def node(kind,name,size,data):
 return kind.encode()+b'\0'+size.to_bytes(3,'little')+petscii(name).ljust(16,b'\xa0')+b'\0'+data
def directory(name,children): return node('D',name,len(children),b''.join(children))
def build(readyos):
 build=ROOT/'build';build.mkdir(exist_ok=True)
 texts=['C64OS 1.09 required','REU limit for ReadyOS interop','Choose the C64OS bank limit:', '32 banks / 2.0000 MB',
 'Cycle: click; Shift-click reverses','1 bank = 64 KB (65536 bytes)', 'Apply / Update', 'Undo patch', 'Confirm', 'Cancel',
 'Checking version and library...', 'ReadyOS must skip 39 banks.', 'Snapshots stay in banks 32-38.',
 'Lower caps leave a gap before 32.', 'Restart C64OS after every change.', 'Undo: boot C64OS independently!']
 rows=[1,2,4,6,8,9,11,11,14,14,16,17,18,19,20,21]
 cols=[2,2,2,2,2,2,2,23,2,23,2,2,2,2,2,2]
 widths=[36]*len(texts);widths[6:10]=[19,14,19,14]
 kinds=[7 if x in (3,6,7,8,9) else 5 for x in range(len(texts))]
 status=['Ready. Choose Apply or Undo.','Blocked: requires C64OS 1.09.','Blocked: unknown REU library.','Disk error; original retained.',
 'Apply selected limit? Confirm / Cancel','Undo? Next boot must be standalone.', 'Verified. Restart C64OS now.', 'WRITE FAILED: restore library backup.']
 data='CONTROL_COUNT = %d\n'%len(texts)
 for name,values in [('rows',rows),('columns',cols),('widths',widths),('kinds',kinds)]:data+=name+': .byte '+','.join(map(str,values))+'\n'
 data+='texts_lo: .lobytes '+','.join('text%d'%i for i in range(len(texts)))+'\ntexts_hi: .hibytes '+','.join('text%d'%i for i in range(len(texts)))+'\n'
 for i,t in enumerate(texts):data+='text%d: '%i+asm(t)
 for name,directive in [('statuses_lo','.lobytes'),('statuses_hi','.hibytes')]: data+=name+': '+directive+' '+','.join('status%d'%i for i in range(len(status)))+'\n'
 for i,t in enumerate(status):data+='status%d: '%i+asm(t)
 data+='sizes: .word '+','.join('size%d'%i for i in range(8,33))+'\n'
 for i in range(8,33):data+='size%d: '%i+asm(f'{i} banks / {i/16:.4f} MB')
 data+='original_status: '+asm('On disk: original capacity detection')
 data+='current_sizes: .word '+','.join('current%d'%i for i in range(8,33))+'\n'
 for i in range(8,33):data+='current%d: '%i+asm(f'On disk: {i} banks / {i/16:.4f} MB')
 (ROOT/'src/ui_data.inc').write_text(data)
 objects=[]
 for name in ['main','os_api','file_stream']:
  obj=build/(name+'.o');objects.append(str(obj))
  subprocess.run(['ca65','-g','-I',str(ROOT/'src'),'-o',str(obj),str(ROOT/'src'/f'{name}.s')],check=True)
 subprocess.run(['ld65','-C',str(ROOT/'tools/app.cfg'),'-Ln',str(build/'main.labels'),'-m',str(build/'main.map'),'-o',str(build/'main.bin'),*objects],check=True)
 body=(build/'main.bin').read_bytes()
 # Keep the full BSS/reference in the PRG; page counts may reserve one extra page.
 files={'main.o':('P',b'\0\x09'+body),'menu.m':('S',petscii('File;a\nQuit:4qQ\n\n')),
        'about.t':('S',petscii('Bridge Setup\n0.1\n2026\nReadyOS\n')),
        'icon.charset':('S',bytes([0xff,0x81,0xbd,0xa5,0xa5,0xbd,0x81,0xff])*9)}
 nodes=[]
 for name,(kind,payload) in files.items():
  (build/name).write_bytes(payload);nodes.append(node(kind,name,len(payload),payload))
 helptext=petscii('C64OS 1.09 only.\nApply a limit of 8-32 banks, then restart.\nReadyOS: 16MB REU, reu_bank_skip=39.\nNormal D81 skips zero: bridge is disabled.\nChanging apps.cfg alone cannot alter the compiled boot skip.\nUndo restores the original capacity detector.\nAfter undo, boot C64OS independently of ReadyOS.\nBackup: system library/bridge.rec (original PRG).\nIf writing fails, restore it as rec.lib.o before reboot.\nReadyOS return utility is installed in system utilities.\nUse Utilities to open it or add it with Menu Editor.\n')
 nodes.append(directory('help',[node('S','toc.t',len(petscii('Setup\n')),petscii('Setup\n')),node('S','Setup',len(helptext)+2,b'\xf6\xf1'+helptext)]))
 utility=(readyos/'bin/c64os-readyos-utility.prg').read_bytes()
 tree=directory('ReadyOS Bridge',[directory('applications',[directory('Bridge Setup',nodes)]),directory('utilities',[node('P','ReadyOS',len(utility),utility)])])
 now=datetime.datetime.now()
 car=b'\x02\xc364\xc1RCHIVE\x03'+bytes([now.year-1900,now.month,now.day,now.hour,now.minute])+petscii('ReadyOS bridge / C64OS 1.09').ljust(31,b'\0')+tree
 car+=binascii.crc32(car).to_bytes(4,'little')
 out=ROOT/'dist/bridge.car';out.parent.mkdir(exist_ok=True);out.write_bytes(car)
 report={'car_sha256':hashlib.sha256(car).hexdigest(),'app_bytes':len(body),'utility_sha256':hashlib.sha256(utility).hexdigest()}
 (build/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--readyos',type=Path,default=ROOT.parents[2]);build(p.parse_args().readyos)
