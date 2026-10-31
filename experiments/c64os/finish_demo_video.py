from pathlib import Path
import json,os,subprocess
p=Path(os.environ.get('READYOS_DEMO_OUT',str(Path(__file__).resolve().parents[3]/'c64os-readyos-experiment/bridge-demo-20260925')))
r=json.loads((p/'capture.json').read_text());d=r['duration_seconds']
assert r['stages'][-1]['label']=='readyos-launcher-final' and not r['errors'] and r['encoder_exit']==0
v=p/'readyos-c64os-tui-reu-demo-silent.mp4';out=p/'readyos-c64os-block-by-block.mp4'
subprocess.run(['/opt/homebrew/bin/ffmpeg','-hide_banner','-loglevel','error','-n','-i',str(v),'-i','/Users/karlprosserpp/Downloads/Block By Block.mp3','-map','0:v:0','-map','1:a:0','-c:v','copy','-af',f'atrim=duration={d},asetpts=PTS-STARTPTS,afade=t=in:st=0:d=1,afade=t=out:st={d-4}:d=4','-c:a','aac','-b:a','192k','-t',str(d),'-movflags','+faststart',str(out)],check=True)
subprocess.run(['/opt/homebrew/bin/ffmpeg','-v','error','-i',str(out),'-f','null','-'],check=True)
print('Soundtrack and complete video verified:',d,'seconds')
