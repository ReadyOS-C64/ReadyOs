"""Terminal-owned hardware transport and passive VIC capture; never polls RAM.
Commands are explicit local JSON jobs. RAM reads require caller confirmation
that the video shows completed loading. Owned uploads refuse existing files.
"""
from pathlib import Path
import socket,struct,time,threading,json,urllib.request,urllib.parse,ftplib,hashlib,os,traceback
from PIL import Image
ROOT=(Path(__file__).resolve().parents[4].parent/'gdos64-bridge'); OUT=ROOT/'evidence/ultimate'; JOBS=OUT/'jobs';JOBS.mkdir(exist_ok=True)
HOST='10.0.0.79';PORT=11000
COLORS=[(0,0,0),(255,255,255),(137,64,54),(122,191,199),(138,70,174),(104,169,65),(62,49,162),(208,220,113),(144,95,37),(92,71,0),(187,119,109),(85,85,85),(128,128,128),(172,234,136),(124,112,218),(171,171,171)]
def api(path,method='GET',data=None):
 with urllib.request.urlopen(urllib.request.Request('http://'+HOST+'/v1/'+path,data=data,method=method),timeout=45) as r:
  b=r.read()
  if 'json' in r.headers.get('Content-Type',''):return json.loads(b)
  return {'hex':b.hex()}
def capture():
 udp=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);udp.setsockopt(socket.SOL_SOCKET,socket.SO_RCVBUF,8*1024*1024);udp.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);udp.bind(('',PORT));udp.setsockopt(socket.IPPROTO_IP,socket.IP_ADD_MEMBERSHIP,socket.inet_aton('239.0.1.64')+socket.inet_aton('0.0.0.0'));udp.settimeout(2)
 route=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);route.connect((HOST,64));receiver=route.getsockname()[0];route.close()
 dest=f'{receiver}:{PORT}'.encode();params=b'\0\0'+dest
 print('STREAM',receiver,api('streams/video:start?ip='+receiver+':'+str(PORT),'PUT'),flush=True)
 current=None; rows={};lastsave=0;count=0
 while True:
  try:d=udp.recv(2048)
  except socket.timeout:continue
  if not (OUT/'packet.json').exists():(OUT/'packet.json').write_text(json.dumps({'length':len(d),'header':d[:12].hex()}))
  if len(d)<12:continue
  seq,frame,lineword,width,n,bits,encoding=struct.unpack('<HHHHBBH',d[:12]);line=lineword&32767
  if (width,n,bits,encoding)!=(384,4,4,0):continue
  if frame!=current:current=frame;rows={}
  rows[line]=d[12:]
  if lineword&32768 and all(i in rows for i in range(0,line+1,4)):
   count+=1
   if time.monotonic()-lastsave<.5:continue
   packed=b''.join(rows[i] for i in range(0,line+1,4));pixels=bytes(c for b in packed for c in (b>>4,b&15))
   im=Image.frombytes('P',(384,line+4),pixels);im.putpalette(bytes(c for rgb in COLORS for c in rgb)+bytes(720))
   im.convert('RGB').save(OUT/'latest.tmp.png');os.replace(OUT/'latest.tmp.png',OUT/'latest.png')
   (OUT/'stream.json').write_text(json.dumps({'frames':count,'time':time.time(),'size':im.size}));lastsave=time.monotonic()
def execute(j):
 op=j['op']
 if op=='api':
  if 'readmem' in j['path']:assert j.get('visually_idle') is True,'Observe completed loading in VIC stream before RAM reads'
  return api(j['path'],j.get('method','GET'))
 if op=='mouse':
  assert j.get('visually_idle') is True
  if not j.get('live'):api('machine:pause','PUT')
  try:
   flag=bytes.fromhex(api('machine:readmem?address=0039&length=1')['hex'])[0]
   xy=struct.pack('<HB',j['x'],j['y']).hex()
   api('machine:writemem?address=003A&data='+xy,'PUT')
   api('machine:writemem?address=8505&data=00','PUT')
   api('machine:writemem?address=0039&data='+f'{flag|32:02x}','PUT')
  finally:
   if not j.get('live'):api('machine:resume','PUT')
  return {'queued':'native GEOS mouse press','x':j['x'],'y':j['y']}
 if op=='upload':
  dst=j['destination'];assert dst.startswith(('/usb1/GD/','/usb1/GEOS20/'))
  local=Path(j['source']);b=local.read_bytes()
  with ftplib.FTP(HOST,timeout=30) as f:
   f.login();folder,name=dst.rsplit('/',1)
   try:f.cwd(folder)
   except ftplib.error_perm:f.mkd(folder);f.cwd(folder)
   assert name.lower() not in [Path(x).name.lower() for x in f.nlst()],'Refusing overwrite'
   with local.open('rb') as src:f.storbinary('STOR '+name,src)
   result=bytearray();f.retrbinary('RETR '+name,result.extend)
  assert bytes(result)==b,'Upload verification failed'
  return {'destination':dst,'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
 if op=='capture':
  import shutil
  p=OUT/j['name'];shutil.copy2(OUT/'latest.png',p);return {'path':str(p)}
 raise ValueError(op)
(OUT/'worker.pid').write_text(str(os.getpid()))
# Passive capture is owned by the already firewall-permitted .NET receiver.
while True:
 for p in sorted(JOBS.glob('*.json')):
  result=p.with_suffix('.result')
  if result.exists():continue
  try:value={'ok':True,'result':execute(json.loads(p.read_text()))}
  except Exception as e:value={'ok':False,'error':repr(e)};traceback.print_exc()
  result.write_text(json.dumps(value,indent=2));print(p.name,value,flush=True)
 time.sleep(.1)
