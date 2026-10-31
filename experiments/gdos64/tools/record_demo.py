"""Capture a verified, real cold/warm UI workflow from the VICE framebuffer."""
import json, os, runpy, shutil, subprocess, threading, time
import probe
from session import ROOT, SOURCE, frame, feed, keys

OUT = ROOT/'evidence/video'
OUT.mkdir(parents=True, exist_ok=True)
FPS = 20
probe.wait_ready('APPLICATIONS:')
im = frame()
log = (OUT/'encoder.log').open('w')
encoder = subprocess.Popen([
    shutil.which('ffmpeg'), '-y', '-hide_banner', '-loglevel', 'warning',
    '-f', 'rawvideo', '-pixel_format', 'rgb24', '-video_size',
    f'{im.width}x{im.height}', '-framerate', str(FPS), '-i', 'pipe:0',
    '-an', '-vf', 'scale=iw*3:ih*3:flags=neighbor', '-c:v', 'libx264',
    '-preset', 'fast', '-crf', '17', '-pix_fmt', 'yuv420p',
    '-movflags', '+faststart', str(OUT/'readyos-gdos64-roundtrip.mp4')
], stdin=subprocess.PIPE, stderr=log)
stop = threading.Event()
errors, stages = [], []
start = time.monotonic()
frames = 0

def capture():
    global frames
    try:
        while not stop.is_set():
            data = frame().tobytes()
            encoder.stdin.write(data)
            frames += 1
            while frames/FPS < time.monotonic()-start-1/FPS:
                encoder.stdin.write(data)
                frames += 1
            stop.wait(max(0, start+frames/FPS-time.monotonic()))
    except BaseException as e:
        errors.append(repr(e))
        stop.set()

def stage(name):
    if errors:
        raise RuntimeError(errors)
    filename = f'{len(stages)+1:02d}-{name}.png'
    im = frame()
    im.resize((im.width*3,im.height*3), resample=0).save(OUT/filename)
    stages.append({'seconds': round(time.monotonic()-start,2),
                   'label': name, 'screenshot': filename})

def paced_feed(data):
    if isinstance(data,str):
        data = data.encode('ascii')
    if len(data)>10:
        for c in data:
            feed(bytes([c]))
            time.sleep(.14)
    else:
        feed(data)

def paced_keys(data):
    if isinstance(data,str):
        data = data.encode('ascii')
    for c in data:
        keys(bytes([c]))
        time.sleep(.08)

probe.feed = paced_feed
probe.keys = paced_keys
probe.record_stage = stage
os.environ['GDOS_BRIDGE_DEMO'] = '1'
thread = threading.Thread(target=capture)
thread.start()
success = False
try:
    runpy.run_path(str(SOURCE/'tools/test_roundtrip.py'),run_name='__main__')
    success = True
    time.sleep(2)
finally:
    stop.set()
    thread.join()
    encoder.stdin.close()
    code = encoder.wait()
    log.close()
    (OUT/'capture.json').write_text(json.dumps({
        'pass':success and not errors and code==0,'fps':FPS,'frames':frames,
        'duration_seconds':frames/FPS,'stages':stages,'errors':errors,
        'encoder_exit':code,
        'capture':'Live VICE framebuffer. Real cold boot and native desk accessory '
                  'round trips; no snapshot restores or program-counter injection. '
                  'Disk loading accelerated with VICE Speed=1000, typing paced '
                  'in wall time. Native GEOS input queues and KERNAL keyboard feed.'
    },indent=2)+'\n')
assert success and not errors and code==0
print(OUT/'readyos-gdos64-roundtrip.mp4')
