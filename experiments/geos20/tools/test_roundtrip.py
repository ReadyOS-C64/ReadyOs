"""GEOS 2.0 real UI roundtrips; no restored emulator snapshots or PC changes."""
from session import *
from PIL import Image
import json,hashlib
REF=SOURCE/'fixtures';OUT=ROOT/'evidence';report={'pass':False,'checks':[],'profile':'VICE PAL 16MiB REU, true1581, GEOS2.0 CBMFiles','input':'KERNAL feed ReadyOS; GEOS native key queue and mouse events at DoCheckButtons'}
def passed(name,**data):
 report['checks'].append(dict(name=name,**{'pass':True},**data));print('PASS',name,flush=True)
 (OUT/'roundtrip-report.json').write_text(json.dumps(report,indent=2)+'\n')
 if 'record_stage' in globals():record_stage(name)
def wait_ready(needle,timeout=120):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  s=screen()
  if needle in s:return
  time.sleep(.25)
 raise AssertionError('Missing ReadyOS text: '+needle+'\n'+s)
def wait_image(name,ref,boxes,timeout=120):
 wanted=Image.open(REF/ref).convert('RGB');end=time.monotonic()+timeout
 while time.monotonic()<end:
  im=frame()
  if all(im.crop(b).tobytes()==wanted.crop(b).tobytes() for b in boxes):
   im.save(OUT/(name+'.png'));return
  time.sleep(.25)
 im.save(OUT/(name+'-failed.png'));raise AssertionError('Image not reached: '+name)
def desktop():wait_image('desktop','desktop.png',[(140,49,191,61),(50,82,270,109)])
def question():wait_image('question','accessory-question.png',[(109,77,282,104),(104,138,278,154)])
def app(n):
 feed(bytes([2]));wait_ready('APPLICATIONS:');feed(bytes([19]+[17]*(n+2)+[13]))
def text_exists(phrase):
 with monitor() as m:assert phrase in m.read(0x400,0x7c00)
try:
 wait_ready('APPLICATIONS:');passed('normal-readyos-boot')
 feed(bytes([19,17,17,13]));wait_ready('TEST BOOT FAILURE')
 feed('S');wait_ready('RESTORED ZP/STACK/SHIM: 1');passed('destructive-ram-proof')
 feed('X');wait_ready('COLD BOOT FAILED: 4');passed('missing-boot-recovery')
 feed('C');desktop();passed('cold-geos-desktop')
 with monitor() as m:
  assert m.read(0xc006,10)==b'GEOS BOOT\x20';assert m.read(0x88c3,1)==b'\x08'
 passed('native-geos20-512k')
 click(12,6);time.sleep(1);click(45,173);question();click(94,110);wait_ready('RESTORED ZP/STACK/SHIM: 2');passed('first-native-return')
 feed('C');desktop();passed('warm-desktop')
 keys('2');time.sleep(1);resource('Speed',100)
 click(220,100);click(32,6);time.sleep(.3);click(48,19);resource('Speed',1000)
 wait_image('write-menu','write-menu.png',[(110,78,278,156)])
 click(103,63);wait_image('write-name','write-name.png',[(111,92,269,108)])
 keys('GEOS BRIDGE\r');wait_image('write-ready','write-ready.png',[(32,35,232,69),(60,75,300,180)])
 phrase=b'GEOS 2.0 document survives the bridge.'
 keys(phrase+b'\rBack to ReadyOS, then back here.');text_exists(phrase);time.sleep(.5)
 before=frame();before.save(OUT/'document-before-test.png');passed('document-created-and-typed')
 click(12,6);time.sleep(.5);click(40,133);question();click(94,110);wait_ready('RESTORED ZP/STACK/SHIM: 3');passed('return-from-live-geowrite')
 parked=reu_image('automated-geos-before-editor');app(1);wait_ready('EDITOR:')
 feed('READYOS AND GEOS 2.0\rBOTH SESSIONS SURVIVE\r');wait_ready('BOTH SESSIONS SURVIVE')
 after=reu_image('automated-geos-after-editor');assert parked[:64*65536]==after[:64*65536]
 passed('geos-reu-preserved',bytes=64*65536,sha256=hashlib.sha256(parked[:64*65536]).hexdigest())
 app(0);wait_ready('START / RESUME');ready=reu_image('automated-ready-before-geos');feed('C')
 end=time.monotonic()+60
 while True:
  try:text_exists(phrase);break
  except AssertionError:
   if time.monotonic()>end:raise
   time.sleep(.25)
 time.sleep(1);resumed=frame();resumed.save(OUT/'document-after-test.png')
 # Compare both text lines, excluding only the blinking caret after line two.
 for box in [(32,72,300,82),(32,83,182,94)]:
  assert before.crop(box).tobytes()==resumed.crop(box).tobytes()
 passed('document-content-and-pixels-restored')
 keys('\rSecond round trip complete.');text_exists(b'Second round trip complete.');passed('typing-after-resume')
 other=reu_image('automated-ready-after-geos');assert ready[72*65536:]==other[72*65536:]
 passed('readyos-app-banks-preserved',bytes=184*65536)
 screenshot(OUT/'document-final.png');report['pass']=True
except BaseException as e:
 report['error']=repr(e)
 try:screenshot(OUT/'roundtrip-failed.png')
 except Exception:pass
 raise
finally:(OUT/'roundtrip-report.json').write_text(json.dumps(report,indent=2)+'\n')
