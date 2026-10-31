import os
"""Real native round trips; only UI input is injected, never code or PC.
Snapshot dumps are read-only evidence for REU ownership checks.
"""
from probe import *
import json,hashlib,traceback
DEMO=os.environ.get('GDOS_BRIDGE_DEMO')=='1'
REF=SOURCE/'fixtures'
report={'pass':False,'checks':[],'input':'VICE KERNAL keyboard feed on ReadyOS; native GEOS key/mouse queues on GDOS','profile':'plain PAL C64, 16 MiB C=REU, two true-emulated C1581 drives'}
def passed(name,**values):
 report['checks'].append({'name':name,'pass':True,**values});print('PASS',name,flush=True)
 if DEMO:
  getattr(sys.modules['probe'],'record_stage',lambda n:None)(name)
  time.sleep(1.5)
def app(index):
 feed(bytes([2]));wait_ready('APPLICATIONS:');feed(bytes([19]+[17]*(index+2)+[13]))
def wait_bytes(needle,timeout=30):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  with monitor() as m:ram=m.read(0x400,0x7c00)
  if needle in ram:return
  time.sleep(.15)
 raise AssertionError('Native app text missing: '+repr(needle))
def question(name):
 resource('Speed',1000)
 wait_image(name,ROOT/'fixtures/accessory-question.png',[(96,67,287,113),(104,139,152,154)],45)
 resource('Speed',100)
def return_ready():
 click(94,110);wait_ready('RESTORED ZP/STACK/SHIM:');resource('Speed',1000)
try:
 wait_ready('APPLICATIONS:');passed('normal-readyos-launcher-boot')
 feed(bytes([19,17,17,13]));wait_ready('TEST BOOT FAILURE')
 with monitor() as m:assert m.read(0xc83b,1)==b'\x47'
 if not DEMO or os.environ.get('GDOS_BRIDGE_FULL_TEST')=='1':
  feed('S');wait_ready('RESTORED ZP/STACK/SHIM: 1');passed('destructive-full-ram-proof')
  feed('X');wait_ready('COLD BOOT FAILED: 4; READYOS RESTORED');passed('missing-boot-file-recovery')
 feed('C');desktop();passed('cold-gdos-desktop')
 with monitor() as m:
  assert m.read(0x88c3,1)==b'\x40';assert m.read(0x9fa9,2)==bytes([63,62])
 passed('native-gdos-4m-layout')
 open_accessory();question('first-native-accessory');return_ready();passed('first-native-return')
 before=reu_image('test-gdos-parked-before-editor')
 app(1);wait_ready('EDITOR:');feed('READYOS AND GDOS64\rSTATE SURVIVES THE SWITCH\r');wait_ready('STATE SURVIVES THE SWITCH')
 screenshot(ROOT/'evidence/test-editor-first.png');after=reu_image('test-gdos-parked-after-editor')
 assert before[:64*65536]==after[:64*65536]
 passed('gdos-reu-preserved-during-readyos-work',bytes=64*65536,sha256=hashlib.sha256(before[:64*65536]).hexdigest())
 app(0);wait_ready('START / RESUME');ready_before=reu_image('test-readyos-parked-before-gdos')
 feed('C');desktop();passed('warm-gdos-desktop')
 # Native application picker, then exact PETSCII disk filename search.
 click(23,191);time.sleep(1);click(10,110);time.sleep(1);click(65,70)
 resource('Speed',1000);time.sleep(2);resource('Speed',100);time.sleep(.5)
 click(220,42);keys('GEOWRITE\r');resource('Speed',1000)
 wait_image('test-geowrite-menu',REF/'write-menu.png',[(109,78,278,156)],60);resource('Speed',100)
 click(103,63);wait_image('test-document-name',REF/'write-name.png',[(110,92,266,108)],20)
 keys('GDOS BRIDGE\r');resource('Speed',1000)
 wait_image('test-document-open',REF/'write-typed.png',[(33,49,350,59),(101,184,283,226)],45);resource('Speed',100)
 phrase=b'GDOS64 document survives the bridge.'
 keys(phrase+b'\rBack to ReadyOS, then back here.');wait_bytes(phrase);time.sleep(.5)
 pre=screenshot(ROOT/'evidence/test-document-before.png');before_pixels=Image.open(pre).crop((32,71,240,81)).tobytes()
 passed('geowrite-created-and-typed-document')
 ready_after=reu_image('test-readyos-parked-after-gdos')
 assert ready_before[72*65536:]==ready_after[72*65536:]
 passed('readyos-app-banks-preserved-during-gdos-work',bytes=184*65536)
 click(12,6);time.sleep(1);click(40,49);question('test-geowrite-accessory');return_ready();passed('return-from-live-geowrite')
 a=reu_image('test-document-parked-before-editor')
 app(1);wait_ready('STATE SURVIVES THE SWITCH');feed('AND THE GDOS DOCUMENT TOO\r');wait_ready('AND THE GDOS DOCUMENT TOO')
 screenshot(ROOT/'evidence/test-editor-second.png');b=reu_image('test-document-parked-after-editor')
 assert a[:64*65536]==b[:64*65536];passed('gdos-document-reu-preserved')
 app(0);wait_ready('START / RESUME');feed('C');resource('Speed',1000);wait_bytes(phrase);resource('Speed',100);time.sleep(1)
 # Pixel check covers the first text line, avoiding blinking cursor/mouse.
 post=screenshot(ROOT/'evidence/test-document-after.png');after_pixels=Image.open(post).crop((32,71,240,81)).tobytes()
 assert before_pixels==after_pixels,'geoWrite first line rendered differently after resume'
 passed('geowrite-document-and-rendering-restored')
 keys('\rSecond round trip complete.');wait_bytes(b'Second round trip complete.')
 screenshot(ROOT/'evidence/test-document-more-typing.png');passed('geowrite-input-after-resume')
 report['pass']=True
except BaseException as e:
 report['error']=repr(e)
 try:screenshot(ROOT/'evidence/test-failure.png')
 except Exception:pass
 raise
finally:
 (ROOT/'evidence'/('demo-roundtrip-report.json' if DEMO else 'roundtrip-report.json')).write_text(json.dumps(report,indent=2)+'\n')
