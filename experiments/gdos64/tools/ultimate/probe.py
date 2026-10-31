"""Read-only Ultimate inventory. Run from a Terminal-owned shell."""
from pathlib import Path
import ftplib,json,urllib.request,time
OUT=(Path(__file__).resolve().parents[4].parent/'gdos64-bridge')/'evidence/ultimate'
OUT.mkdir(parents=True,exist_ok=True)
results={}
for key,path in [('version','version'),('drives','drives'),('config_names','configs'),('configs','configs/*/*')]:
 try:
  with urllib.request.urlopen('http://10.0.0.79/v1/'+path,timeout=15) as r:results[key]=json.load(r)
 except Exception as e: results[key]={'error':str(e)}
 (OUT/'inventory.json').write_text(json.dumps(results,indent=2))
try:
 with ftplib.FTP('10.0.0.79',timeout=15) as ftp:
  ftp.login();results['ftp_root']=ftp.nlst('/');results['usb1']=ftp.nlst('/usb1')
  for directory in ('GD','GEOS20'):
   try:results[directory]=ftp.nlst('/usb1/'+directory)
   except ftplib.error_perm as e:results[directory]={'error':str(e)}
except Exception as e:results['ftp_error']=str(e)
(OUT/'inventory.json').write_text(json.dumps(results,indent=2))
print(json.dumps(results,indent=2))
