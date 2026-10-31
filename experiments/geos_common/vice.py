"""Isolated VICE launcher and monitor helpers for this GDOS64 lab."""
from pathlib import Path
import socket,struct,time,os,subprocess
from contextlib import contextmanager
from monitor import Monitor
ROOT=Path(__file__).resolve().parents[1]
BPORT=int(os.environ.get('GDOS_BINARY_PORT','6520'))
TPORT=int(os.environ.get('GDOS_TEXT_PORT','6620'))
@contextmanager
def monitor():
 with socket.create_connection(('127.0.0.1',BPORT),5) as s:
  s.settimeout(15);m=Monitor(s)
  try:yield m
  finally:
   try:m.command(0xaa)
   except (OSError,RuntimeError):pass

def text(*lines):
 with socket.create_connection(('127.0.0.1',TPORT),5) as s:
  s.settimeout(.15)
  def drain():
   data=b''
   while True:
    try:
     b=s.recv(65536)
     if not b:break
     data+=b
    except socket.timeout:break
   return data.decode(errors='replace')
  drain();out=[]
  for line in lines:s.sendall((line+'\n').encode());out.append(drain())
  s.sendall(b'x\n');drain()
  return '\n'.join(out)
def screenshot(path):
 path=Path(path).resolve();path.parent.mkdir(parents=True,exist_ok=True)
 if path.exists():path.unlink()
 output=text(f'screenshot "{path}" 2')
 if not path.exists():raise RuntimeError('Screenshot failed: '+output)
 return path

def click(x,y,double=False):
 for _ in range(2 if double else 1):
  with monitor() as m:
   m.write(0x3a,struct.pack('<HB',x,y));m.write(0x8505,b'\0');m.write(0x39,bytes([m.read(0x39,1)[0]|32]))
  time.sleep(.06)
  with monitor() as m:
   m.write(0x8505,b'\x80');m.write(0x39,bytes([m.read(0x39,1)[0]|32]))
  time.sleep(.06)
def keys(value):
 # Native GDOS ring buffer, per NewKeyInBuf/GetKeyFromBuf; not KERNAL $C6.
 for key in (value.encode('ascii') if isinstance(value,str) else value):
  with monitor() as m:
   tail=m.read(0x87d8,1)[0];head=m.read(0x87d7,1)[0];n=(tail+1)%16
   if n==head:raise RuntimeError('Native input queue full')
   m.write(0x87da+tail,bytes([key]));m.write(0x87d8,bytes([n]));m.write(0x39,bytes([m.read(0x39,1)[0]|128]))
  time.sleep(.05)
def wake():
 text('resourceset "JoyPort1Device" "37"')
 for value in (16,0):
  with monitor() as m:m.command(0xa2,struct.pack('<HH',1,value))
  time.sleep(.15)
 text('resourceset "JoyPort1Device" "3"')
def args(system=None,second=None,reu=None):
 out=['x64sc','-default','+saveres','+confirmonexit','-reu','-reusize','16384','-drive8type','1581','-drive9type','1581','-drive8truedrive','-drive9truedrive','+trapdevice8','+trapdevice9','-controlport1device','3','-controlport2device','0','-mouse','-binarymonitor','-binarymonitoraddress',f'127.0.0.1:{BPORT}','-remotemonitor','-remotemonitoraddress',f'127.0.0.1:{TPORT}','-8',str(system or ROOT/'runtime/gdos-system.d81'),'-9',str(second or ROOT/'runtime/community-apps.d81'),'-keybuf','load"gd",8,1\\n','+warp','+sound']
 if reu:out+=['-reuimage',str(reu),'-reuimagerw']
 return out
if __name__=='__main__':os.execvp('x64sc',args())
