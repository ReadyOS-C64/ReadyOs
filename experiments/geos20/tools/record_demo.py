"""Record the complete real GEOS bridge test; never restore emulator snapshots."""
import json, runpy, shutil, subprocess, threading, time
import session
from session import ROOT, frame
OUT=ROOT/'evidence/video';OUT.mkdir(parents=True,exist_ok=True)
FPS=20
im=frame();log=(OUT/'encoder.log').open('w')
encoder=subprocess.Popen([shutil.which('ffmpeg'),'-y','-hide_banner','-loglevel','warning','-f','rawvideo','-pixel_format','rgb24','-video_size',f'{im.width}x{im.height}','-framerate',str(FPS),'-i','pipe:0','-an','-vf','scale=iw*3:ih*3:flags=neighbor','-c:v','libx264','-preset','fast','-crf','17','-pix_fmt','yuv420p','-movflags','+faststart',str(OUT/'readyos-geos20-proof.mp4')],stdin=subprocess.PIPE,stderr=log)
stop=threading.Event();errors=[];stages=[];start=time.monotonic();frames=0
original_feed=session.feed;original_keys=session.keys

def capture():
 global frames
 try:
  while not stop.is_set():
   data=frame().tobytes();encoder.stdin.write(data);frames+=1
   while frames/FPS<time.monotonic()-start-1/FPS:
    encoder.stdin.write(data);frames+=1
   stop.wait(max(0,start+frames/FPS-time.monotonic()))
 except BaseException as e:errors.append(repr(e));stop.set()

def stage(name):
 filename=f'{len(stages)+1:02d}-{name}.png';im=frame();im.resize((im.width*3,im.height*3),resample=0).save(OUT/filename)
 stages.append(dict(seconds=round(time.monotonic()-start,2),label=name,screenshot=filename));time.sleep(2)

def feed(data):
 if isinstance(data,str):data=data.encode('ascii')
 if len(data)>10:
  for c in data:original_feed(bytes([c]));time.sleep(.14)
 else:original_feed(data)

def keys(data):
 if isinstance(data,str):data=data.encode('ascii')
 for c in data:original_keys(bytes([c]));time.sleep(.06)

session.record_stage=stage;session.feed=feed;session.keys=keys
thread=threading.Thread(target=capture);thread.start();success=False
try:
 runpy.run_path(str(SOURCE/'tools/test_roundtrip.py'),run_name='__main__');success=True;time.sleep(3)
finally:
 stop.set();thread.join();encoder.stdin.close();code=encoder.wait();log.close()
 (OUT/'capture.json').write_text(json.dumps(dict(pass_=success and not errors and code==0,fps=FPS,frames=frames,duration_seconds=frames/FPS,stages=stages,errors=errors,encoder_exit=code,capture='Live VICE framebuffer. Native GEOS desk accessory, real cold and warm transitions. No snapshot restore or program-counter changes. Accelerated disk loading; typing paced in wall time.'),indent=2)+'\n')
assert success and not errors and code==0
