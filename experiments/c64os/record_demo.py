"""Record actual VICE framebuffer samples while driving native UI round trips."""
import json,os,socket,struct,subprocess,sys,threading,time
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'experiments/c64os'))
from verify_bridge_ui import Monitor,feed,screen,wait_screen
from verify_roundtrip_ui import launcher_app,mouse
OUT=Path(os.environ.get('READYOS_DEMO_OUT', str(ROOT.parent/'c64os-readyos-experiment/bridge-demo-20260925')))
OUT.mkdir(parents=True, exist_ok=True)
FPS=25
lock=threading.RLock(); stop=threading.Event(); errors=[]; shots=[]; stages=[]
sock=socket.create_connection(('127.0.0.1',6511),5);sock.settimeout(15);m=Monitor(sock)

def warp(value):
 sys.path.insert(0,str(ROOT.parent/'agenticdevharness/tools'))
 from vice_readyshell_automation import Monitor as TextMonitor
 t=TextMonitor('127.0.0.1',6611)
 try:
  t.cmd('warp '+('on' if value else 'off')+'\n');t.cmd('x\n')
 finally:t.close()

def frame():
 p=m.command(0x84,b'\1\0')
 fl,w,h,x,y,iw,ih,bpp,n=struct.unpack_from('<I6HBI',p)
 assert bpp==8 and n==w*h and fl==13
 cw,ch=iw+64,ih+72
 rows=[p[21+row*w+x-32:21+row*w+x+iw+32] for row in range(y-36,y+ih+36)]
 assert all(len(row)==cw for row in rows)
 im=Image.frombytes('P',(cw,ch),b''.join(rows));im.putpalette(palette)
 return im.convert('RGB')

wait_screen(m,'APPLICATIONS')
from verify_roundtrip_ui import native_command
warp(True)
p=m.command(0x91,b'\1');count=struct.unpack_from('<H',p)[0];pos=2;palette=[]
for _ in range(count):
 size=p[pos];palette.extend(p[pos+1:pos+4]);pos+=size+1
palette += [0]*(768-len(palette))
im=frame();m.command(0xaa);width,height=im.size
log=(OUT/'ffmpeg.log').open('w')
enc=subprocess.Popen(['/opt/homebrew/bin/ffmpeg','-y','-hide_banner','-loglevel','warning','-f','rawvideo','-pixel_format','rgb24','-video_size',f'{width}x{height}','-framerate',str(FPS),'-i','pipe:0','-an','-vf','scale=iw*3:ih*3:flags=neighbor','-c:v','libx264','-preset','fast','-crf','17','-pix_fmt','yuv420p','-movflags','+faststart',str(OUT/'readyos-c64os-tui-reu-demo-silent.mp4')],stdin=subprocess.PIPE,stderr=log)
start=time.monotonic();frames=0

def recording():
 global frames
 try:
  while not stop.is_set():
   with lock:
    data=frame().tobytes();m.command(0xaa)
   enc.stdin.write(data);frames+=1
   # Duplicate only to retain wall-clock duration if a monitor request stalls.
   while frames/FPS < time.monotonic()-start-1/FPS:
    enc.stdin.write(data);frames+=1
   stop.wait(max(0,start+frames/FPS-time.monotonic()))
 except BaseException as e:errors.append(repr(e));stop.set()

thread=threading.Thread(target=recording);thread.start()

def action(fn,*args):
 with lock:fn(m,*args)

def wait(text,timeout=40):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  if errors:raise RuntimeError(errors)
  with lock:
   value=screen(m);m.command(0xaa)
  if text in value:return value
  time.sleep(.15)
 raise RuntimeError('Missing '+text+'\n'+value)

def shot(label):
 with lock:
  im=frame();m.command(0xaa)
 name=f'{len(shots)+1:02d}-{label}.png';im.resize((width*3,height*3),Image.Resampling.NEAREST).save(OUT/name)
 shots.append(name);stages.append({'seconds':round(time.monotonic()-start,2),'label':label,'screenshot':name})
 print(label,flush=True)

def key(c):
 end=time.monotonic()+15
 while True:
  with lock:
   empty=m.read(0xc6,1)==b'\0';m.command(0xaa)
  if empty:break
  if time.monotonic()>end:raise RuntimeError('Key queue did not drain')
  time.sleep(.04)
 with lock:
  m.write(0x277,bytes([c]));m.write(0xc6,b'\1');m.command(0xaa)
 time.sleep(.1)

def keys(data):
 for c in data:key(c)

def editor_type(data):
 # Pace native keyboard events in wall time even while VICE is in warp.
 for index,c in enumerate(data):
  action(feed,[c]);time.sleep((.13,.18,.15,.20,.14)[index%5])

def editor():
 action(feed,[19,17,17,17,13]);wait('EDITOR:')

def bridge():
 action(feed,[19,17,17,13]);wait('START / RESUME C64OS (C)')

def tasks_open():
 text=wait('DEVICES')
 rows=[i for i,line in enumerate(text.splitlines()) if 'TASKS ' in line[13:]]
 assert len(rows)==1,repr(rows)
 action(mouse,164,rows[0]*8+4,[1,3,4,1,3,5])
 wait('SYSTEM FILE TASK VIEW');time.sleep(1)
 # An existing frozen app can leave its normal Switcher utility open.
 with lock:
  active=m.read(0x3fe,1);m.command(0xaa)
 if active!=b'\0':
  action(native_command,b'W',2);time.sleep(.8)

try:
 shot('preloaded-readyos-apps');time.sleep(1.5)
 bridge();time.sleep(.7);shot('c64os-bridge-menu');time.sleep(1.5)
 # No C64OS context has been created: record the actual first disk boot.
 with lock:warp(False)
 action(feed,[13]);time.sleep(8);shot('c64os-initial-load')
 wait('DEVICES',90);wait('OFFLINE')
 with lock:warp(True)
 time.sleep(.7);shot('c64os-file-manager')
 tasks_open()
 keys(b'\rDEMO THE BRIDGE TO #READYOS\r')
 keys(b'\rSWITCH TO C64OS #BRIDGE\r');action(native_command,b'I',2);time.sleep(.3)
 keys(b'\rRETURN TO #READYOS\r')
 keys(b'\rRESUME C64OS #BRIDGE\r')
 wait('RESUME C64OS #BRIDGE');shot('tasks-outline-and-hashtags');time.sleep(1.5)
 keys(bytes([145,145,32]));time.sleep(.6);shot('task-completed');time.sleep(1)
 keys(bytes([17])+b'N');time.sleep(.5)
 keys(b'RETURN TO READYOS.\rRESUME THIS TASK IN C64OS.')
 wait('RESUME THIS TASK IN C64OS.');time.sleep(.5);shot('task-note-added');time.sleep(1.5)
 action(native_command,bytes([3]),0);wait('RETURN NEW');time.sleep(.5)
 keys(bytes([17]));shot('tasks-with-note-and-completion');time.sleep(1.5)
 action(mouse,1,4,[1]);wait('READYOS');time.sleep(.5);shot('return-to-readyos-menu')
 action(mouse,10,12,[2,3]);wait('RESTORED ZP/STACK/SHIM: 1');time.sleep(.6);shot('bridge-first-return')
 action(feed,[137]);wait('EDITOR:');time.sleep(.3)
 editor_type(b'READYOS AND C64OS\rBACK FROM THE FIRST ROUND TRIP\r')
 wait('BACK FROM THE FIRST ROUND TRIP');time.sleep(.5);shot('preloaded-editor');time.sleep(1.5)
 action(feed,[137]);wait('REU MEMORY MAP');wait('PHYS $00');time.sleep(.25);shot('reu-skipped-banks');time.sleep(.7)
 action(feed,[17,17]);wait('PHYS $20');time.sleep(.25);shot('reu-bridge-snapshot-range');time.sleep(.7)
 action(feed,[29]*7);wait('PHYS $27');wait('READYOS BANK');time.sleep(.25);shot('reu-readyos-control-bank');time.sleep(.7)
 action(feed,[29]*3);wait('PHYS $2A');wait('C64OS');time.sleep(.25);shot('reu-bridge-app-bank');time.sleep(1)
 action(feed,[137]);wait('START / RESUME C64OS (C)');time.sleep(.5);shot('back-to-bridge')
 action(feed,[19,13]);wait('DEMO THE BRIDGE TO #READYOS');time.sleep(.6);shot('tasks-state-after-round-trip');time.sleep(1)
 # Reopen the note on the preceding item; then leave focus on another item.
 keys(bytes([145])+b'N');wait('RESUME THIS TASK IN C64OS.');time.sleep(.6);shot('note-survived-round-trip');time.sleep(1)
 keys(bytes([17,147])+b'\rSECOND ROUND TRIP COMPLETE.');wait('SECOND ROUND TRIP COMPLETE.');time.sleep(.5);shot('note-updated-on-second-visit')
 action(native_command,bytes([3]),0);wait('RETURN NEW');time.sleep(.3)
 keys(bytes([17])+b'\rSWITCH BACK AGAIN #READYOS\r');time.sleep(.3);keys(b' ')
 keys(bytes([145]));time.sleep(.5);shot('second-completed-task');time.sleep(1.5)
 action(mouse,1,4,[1]);wait('READYOS');time.sleep(.5)
 action(mouse,10,12,[2,3]);wait('RESTORED ZP/STACK/SHIM: 1');time.sleep(.6);shot('bridge-second-return')
 action(feed,[137]);wait('BACK FROM THE FIRST ROUND TRIP');time.sleep(.3)
 editor_type(b'TWO ROUND TRIPS COMPLETE\r');wait('TWO ROUND TRIPS COMPLETE');time.sleep(.4);shot('editor-state-preserved');time.sleep(1.5)
 action(feed,[137]);wait('REU MEMORY MAP');wait('PHYS $2A');time.sleep(.3);shot('reu-bridge-still-present');time.sleep(.8)
 action(feed,[2]);wait('APPLICATIONS');time.sleep(.5);shot('readyos-launcher-final');time.sleep(1.5)

finally:
 stop.set();thread.join();enc.stdin.close();code=enc.wait();log.close()
 with lock:warp(True)
 sock.close()
 (OUT/'capture.json').write_text(json.dumps({'fps':FPS,'frames':frames,'duration_seconds':frames/FPS,'capture':'Live VICE framebuffer sampled through binary monitor; native UI navigation; one cold and one warm round trip; initial C64OS load at normal speed, then warp and traps enabled; human-paced ReadyOS typing; TUI menu; preloaded Editor and REU Viewer; Tasks indentation, hashtags, completion and notes; no snapshot restores','stages':stages,'errors':errors,'encoder_exit':code},indent=2)+'\n')
assert not errors and code==0
print('Captured',frames,'frames:',frames/FPS,'seconds',flush=True)
