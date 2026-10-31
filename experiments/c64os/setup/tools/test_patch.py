#!/usr/bin/env python3
"""Execute assembled validation and real patch/undo paths under a CPU model.
Only OS disk transport is stubbed; all verification, patching, rollback code runs.
"""
from pathlib import Path
import sys, binascii
from py65.devices.mpu6502 import MPU
ROOT=Path(__file__).resolve().parents[1]
labels={p[2].lstrip('.'):int(p[1],16) for l in (ROOT/'build/main.labels').read_text().splitlines() if len(p:=l.split())==3}
raw=(ROOT/'build/main.bin').read_bytes()
original=(ROOT.parents[2].parent/'c64os-readyos-experiment/baseline/os/LIBRARY/rec.lib.o.P00').read_bytes()[26:]
def call(cpu,name,limit=300000):
 cpu.pc=labels[name];cpu.sp=0xfd;cpu.memory[0x1fe]=0xfe;cpu.memory[0x1ff]=0x7f
 for _ in range(limit):
  if cpu.pc==0x7fff:return
  cpu.step()
 raise AssertionError('CPU timeout '+name)
def validate(blob):
 c=MPU();c.memory[0x900:0x900+len(raw)]=raw
 c.memory[labels['image']:labels['image']+len(blob)]=blob
 c.memory[labels['file_size']]=len(blob)&255
 call(c,'validate_library')
 return not(c.p&1),c.memory[labels['installed_banks']],c
assert len(original)==217
assert validate(original)[:2]==(True,0)
for banks in range(8,33):
 p=bytearray(original);p[0x6f:0x75]=bytes([0xa9,banks,0x8d,0x81,2,0x60]);ok,n,c=validate(p)
 assert ok and n==banks
 assert bytes(c.memory[labels['image']:labels['image']+217])==original
for banks in [0,1,7,33,255]:
 p=bytearray(original);p[0x6f:0x75]=bytes([0xa9,banks,0x8d,0x81,2,0x60]);assert not validate(p)[0]
for i in range(217):
 p=bytearray(original);p[i]^=1;assert not validate(p)[0],i
for n in [0,1,216,218,255]:assert not validate((original+bytes(40))[:n])[0]
print('PASS: original, 25 caps, undo normalization, invalid caps, all 217 single-byte corruptions, wrong lengths')

def transaction(files,operation,banks=32,fail_write=None):
 c=MPU();c.memory[0x900:0x900+len(raw)]=raw
 c.memory[labels['operation']]=operation;c.memory[labels['selected_banks']]=banks
 state={};writes=[]
 def ret(carry=False):
  c.p=(c.p&~1)|int(carry);lo=c.memory[0x100+((c.sp+1)&255)];hi=c.memory[0x100+((c.sp+2)&255)];c.sp=(c.sp+2)&255;c.pc=((hi<<8)|lo)+1
 def text(addr):
  out=bytearray()
  while c.memory[addr]:out.append(c.memory[addr]);addr+=1
  return out.decode('latin1')
 c.pc=labels['change_library'];c.sp=0xfd;c.memory[0x1fe]=0xfe;c.memory[0x1ff]=0x7f
 failed=False
 for _ in range(1000000):
  pc=c.pc
  if pc==0x7fff:return files,writes,c.memory[labels['status_index']]
  if pc==labels['getsfref']:
   assert c.y==labels['fileref']//256,'getsfref must receive our owned destination page'
   dest=c.y*256;c.memory[dest:dest+256]=[0]*256;c.memory[dest]=12;c.memory[dest+1]=1
   c.memory[dest+22:dest+28]=b'//OS/\0';c.x=c.y;c.y=27;ret();continue
  if pc==labels['stream_open']:
   ref=c.x+(c.y<<8);name=text(ref+5);state={'name':name,'mode':c.a,'pos':0,'data':bytearray()}
   c.memory[labels['stream_eof']]=0
   ret(not(c.a&4) and name not in files);continue
  if pc==labels['stream_read']:
   data=files.get(state['name'],b'');idx=state['pos']
   if idx>=len(data):ret(True);continue
   c.a=data[idx];state['pos']+=1;c.memory[labels['stream_eof']]=int(state['pos']==len(data));ret();continue
  if pc==labels['stream_write']:
   state['data'].append(c.a);ret();continue
  if pc==labels['stream_close']:
   err=False
   if state.get('mode',0)&4:
    name=state['name'];writes.append(name);files[name]=bytes(state['data'])
    if fail_write==name and not failed:
     failed=True;files[name]=b'broken';err=True
   ret(err);continue
  c.step()
 raise AssertionError('transaction CPU timeout '+hex(c.pc))
for banks in (8,16,31,32):
 fs={'VERSION.T':b'1.09\r','REC.LIB.O':original}
 fs,writes,status=transaction(fs,2,banks)
 expect=bytearray(original);expect[0x6f:0x75]=bytes([0xa9,banks,0x8d,0x81,2,0x60])
 assert fs['REC.LIB.O']==expect and fs['BRIDGE.REC']==original and status==6,(banks,status,writes)
 fs,writes,status=transaction(fs,3)
 assert fs['REC.LIB.O']==original and status==6
for version in (b'1.08\r',b'1.10\r',b'1.09X',b'1.09\rX',b''):
 fs,writes,status=transaction({'VERSION.T':version,'REC.LIB.O':original},2)
 assert not writes and status==1
fs,writes,status=transaction({'VERSION.T':b'1.09\r','REC.LIB.O':original},2,16,'REC.LIB.O')
assert fs['REC.LIB.O']==original and status==3,(status,writes)
print('PASS: assembled native apply/update/undo, version rejection without writes, failed-write rollback')
