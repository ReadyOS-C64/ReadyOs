"""Experimental bridge session tools. Never target the original lab ports."""
from pathlib import Path
import os,sys,time,struct
SOURCE=Path(__file__).resolve().parents[1]
READY=SOURCE.parents[1]
ROOT=Path(os.environ.get('GDOS_BRIDGE_PROJECT',str(READY.parent/'gdos64-bridge')))
sys.path.insert(0,str(READY/'experiments/geos_common'))
os.environ['GDOS_BINARY_PORT']='6540';os.environ['GDOS_TEXT_PORT']='6640'
import vice, socket, threading, atexit
from contextlib import contextmanager
from monitor import Monitor
lock=threading.RLock()
_sock=None
_mon=None
@contextmanager
def monitor():
 global _sock,_mon
 with lock:
  if _mon is None:
   _sock=socket.create_connection(('127.0.0.1',6540),5)
   _sock.settimeout(15);_mon=Monitor(_sock)
  try:yield _mon
  finally:_mon.command(0xaa)
def close_monitor():
 global _sock,_mon
 if _sock is not None:
  try:
   _mon.command(0xaa)
   # Allow VICE to leave the monitor before closing its controlling socket.
   time.sleep(.15)
  except (OSError,RuntimeError):pass
  _sock.close();_sock=None;_mon=None
atexit.register(close_monitor)
vice.monitor=monitor
def click(x,y,double=False):
 # GDOS 1.03 ExecMseKeyb is a native foreground input boundary at $FD68.
 # Wait for real execution, then enqueue a press. The native 1351 driver
 # observes the physically released button and supplies the release itself.
 # Do not enqueue a release after a command that can switch into ReadyOS.
 # Temporary checkpoints do not change code or the program counter.
 for _ in range(2 if double else 1):
  with monitor() as m:
   assert m.read(0xfd68,4)==bytes([0x24,0x39,0x50,0x0f])
   m.checkpoint(0xfd68);m.command(0xaa);m.wait(0xfd68)
   m.write(0x3a,struct.pack('<HB',x,y))
   m.write(0x8505,bytes([0]))
   m.write(0x39,bytes([m.read(0x39,1)[0]|32]))
  time.sleep(.2)

def keys(value):
 # GDOS can still be finishing native application restore when its bitmap is
 # visible. Respect the 16-byte input queue instead of overflowing it.
 for key in (value.encode('ascii') if isinstance(value,str) else value):
  deadline=time.monotonic()+30
  while True:
   with monitor() as m:
    tail=m.read(0x87d8,1)[0];head=m.read(0x87d7,1)[0];n=(tail+1)%16
    if n!=head:
     m.write(0x87da+tail,bytes([key]));m.write(0x87d8,bytes([n]))
     m.write(0x39,bytes([m.read(0x39,1)[0]|128]))
     break
   if time.monotonic()>deadline:raise RuntimeError('GDOS input queue did not drain')
   time.sleep(.05)
  time.sleep(.08)
_palette=None
def resource(name,value):
 n=name.encode()
 with monitor() as m:
  m.command(0x52,bytes([1,len(n)])+n+bytes([4])+int(value).to_bytes(4,'little'))
def text(*commands):
 for cmd in commands:
  if cmd in ('warp on','warp off'):resource('Speed',1000 if cmd.endswith('on') else 100)  # VICE 3.10 exposes speed, not WarpMode
  else:raise ValueError('Use binary monitor: '+cmd)
 return ''
def frame():
 global _palette
 from PIL import Image
 with monitor() as m:
  if _palette is None:
   p=m.command(0x91,b'\1');count=struct.unpack_from('<H',p)[0];pos=2;_palette=[]
   for _ in range(count):
    size=p[pos];_palette.extend(p[pos+1:pos+4]);pos+=size+1
   _palette += [0]*(768-len(_palette))
  p=m.command(0x84,b'\1\0')
  fl,w,h,x,y,iw,ih,bpp,n=struct.unpack_from('<I6HBI',p)
  assert bpp==8 and n==w*h and fl==13
  rows=[p[21+r*w+x-32:21+r*w+x+iw+32] for r in range(y-36,y+ih+36)]
  im=Image.frombytes('P',(iw+64,ih+72),b''.join(rows));im.putpalette(_palette)
 return im.convert('RGB')
def screenshot(path):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True);frame().save(path);return path



def feed(data):
 if isinstance(data,str):data=data.encode('ascii')
 with monitor() as m:m.command(0x72,bytes([len(data)])+bytes(data))

def screen():
 with monitor() as m:b=m.read(0x400,1000)
 def ch(c):
  c&=127
  return chr(c+64) if c<32 else chr(c) if c<96 else '.'
 return '\n'.join(''.join(ch(c) for c in b[i:i+40]) for i in range(0,1000,40))
if __name__=='__main__':
 if len(sys.argv)>1:
  print(text(*sys.argv[1:]))
 else:print(screen())

def reu_image(name):
 """Read-only evidence snapshot; this project never calls VICE undump."""
 p=ROOT/'evidence'/(name+'.vsf');b=str(p).encode()
 with monitor() as m:m.command(0x41,bytes([0,0,len(b)])+b)
 data=p.read_bytes();pos=58
 while pos<len(data):
  label=data[pos:pos+16].split(b'\0')[0];size=struct.unpack_from('<I',data,pos+18)[0]
  assert size>=22
  if label==b'REU1764':
   assert data[pos+16:pos+18]==b'\0\0'
   ram=data[pos+42:pos+size];assert len(ram)==16*1024*1024;return ram
  pos+=size
 raise AssertionError('REU absent from evidence snapshot')
