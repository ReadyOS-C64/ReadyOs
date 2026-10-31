from session import *
from PIL import Image

def wait_ready(needle,timeout=120):
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  s=screen()
  if needle in s:return s
  time.sleep(.3)
 raise AssertionError('Did not reach '+needle+'\n'+s)

def desktop(timeout=180):
 boxes=[(48,46,70,62),(32,220,77,234)]
 refim=Image.open(ROOT/'fixtures/desktop.png').convert('RGB')
 ref=[refim.crop(b).tobytes() for b in boxes]
 deadline=time.monotonic()+timeout
 while time.monotonic()<deadline:
  p=screenshot(ROOT/'evidence/probe-desktop.png')
  im=Image.open(p).convert('RGB')
  if all(im.crop(b).tobytes()==r for b,r in zip(boxes,ref)):
   text('warp off');time.sleep(.5);return
  time.sleep(.3)
 raise AssertionError('GDOS desktop not reached')

def open_accessory():
 click(23,191);time.sleep(1);click(10,110);time.sleep(1);click(65,98)
 text('warp on');time.sleep(2);text('warp off');time.sleep(.5)
 click(65,42);time.sleep(1);click(261,158)
 text('warp on');time.sleep(2);text('warp off');time.sleep(.5)
 screenshot(ROOT/'evidence/probe-accessory.png')

if __name__=='__main__':
 wait_ready('APPLICATIONS:');feed(bytes([19,17,17,13]));wait_ready('TEST BOOT FAILURE')
 feed('C');desktop();open_accessory()
 print('Native accessory opened; inspect evidence/probe-accessory.png')

def wait_image(name,reference,boxes,timeout=120):
 ref=Image.open(reference).convert('RGB');pixels=[ref.crop(b).tobytes() for b in boxes]
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  p=screenshot(ROOT/'evidence'/f'{name}.png');im=Image.open(p).convert('RGB')
  if all(im.crop(b).tobytes()==v for b,v in zip(boxes,pixels)):
   return p
  time.sleep(.2)
 raise AssertionError('Image not reached: '+name)
