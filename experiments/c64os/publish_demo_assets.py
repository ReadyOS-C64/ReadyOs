"""Add the completed capture to the existing Site; hosting runs separately."""
import html,json,os,shutil,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
source=Path(os.environ.get('READYOS_DEMO_OUT',str(ROOT.parent/'c64os-readyos-experiment/bridge-demo-20260925')))
site_root=Path(os.environ.get('READYOS_DEMO_SITE',str(ROOT.parent/'readyos-c64os-demo'))).expanduser().resolve()
site=site_root/'dist'
assert (site_root/'.openai/hosting.json').is_file() and (site/'index.html').is_file(), 'Expected the existing demo Site project'
r=json.loads((source/'capture.json').read_text())
assert not r['errors'] and r['encoder_exit']==0
assert r['stages'][-1]['label']=='readyos-launcher-final'
folder='bridge-demo';dest=site/folder;dest.mkdir(exist_ok=True)
video='readyos-c64os-block-by-block.mp4'
shutil.copy2(source/video,dest/video)
labels={
'preloaded-readyos-apps':'ReadyOS launcher: c64os bridge',
'c64os-bridge-menu':'The centered c64os bridge menu',
'c64os-initial-load':'The first C64OS load',
'c64os-file-manager':'C64OS File Manager',
'tasks-outline-and-hashtags':'Demo the bridge to #readyos',
'task-completed':'First switching task completed',
'task-note-added':'Adding a note about the round trip',
'tasks-with-note-and-completion':'Indented tasks with completion and a note',
'return-to-readyos-menu':'Returning to ReadyOS',
'bridge-first-return':'First round trip complete',
'preloaded-editor':'The preloaded ReadyOS Editor',
'reu-skipped-banks':'REU Viewer: skipped banks',
'reu-bridge-snapshot-range':'Bridge snapshot range',
'reu-readyos-control-bank':'ReadyOS control bank: 39',
'reu-bridge-app-bank':'Bridge app state: physical bank 42',
'back-to-bridge':'Back to the bridge',
'tasks-state-after-round-trip':'Tasks retained across the switch',
'note-survived-round-trip':'The task note survives the round trip',
'note-updated-on-second-visit':'Adding another line to the note',
'second-completed-task':'Another task completed; selection moved away',
'bridge-second-return':'Second round trip complete',
'editor-state-preserved':'Editor text retained across both trips',
'reu-bridge-still-present':'Bridge state remains in REU',
'readyos-launcher-final':'Back at the ReadyOS launcher',
}
figures=[]
with zipfile.ZipFile(dest/'screenshots.zip','w',zipfile.ZIP_DEFLATED) as z:
 for stage in r['stages']:
  name=stage['screenshot'];shutil.copy2(source/name,dest/name);z.write(source/name,name)
  label=html.escape(labels[stage['label']]);asset=f'{folder}/{name}'
  figures.append(f'<figure><a href="{asset}"><img loading="lazy" width="1152" height="816" src="{asset}" alt="{label}"></a><figcaption>{stage["seconds"]:.1f}s · {label}</figcaption></figure>')
new=f'''<section id="bridge-demo"><h2>Cold start and two round trips</h2><p>Watch C64OS load for the first time, then switch between its Tasks app and ReadyOS. The indented list tracks the round trips, with hashtags, completed tasks, and a note edited on the second visit. Editor and REU Viewer are preloaded; the REU walkthrough is quicker. The initial C64OS load runs at normal speed, followed by warp with drive traps enabled. ReadyOS Editor typing is slowed to a natural pace.</p><video controls playsinline preload="metadata" poster="{folder}/02-c64os-bridge-menu.png"><source src="{folder}/{video}" type="video/mp4"></video><p>{r['duration_seconds']:.1f} seconds · Soundtrack: Block By Block · <a href="{folder}/{video}" download>Download video</a> · <a href="{folder}/screenshots.zip" download>Download screenshots</a></p><details><summary>View {len(r['stages'])} screenshots from this demo</summary><div class="gallery">{''.join(figures)}</div></details></section>'''
page=site/'index.html';s=page.read_text()
if '<section id="bridge-demo">' in s:
 a=s.index('<section id="bridge-demo">');b=s.index('</section>',a)+len('</section>');s=s[:a]+new+s[b:]
else:s=s.replace('<section id="polished-demo">',new+'<section id="polished-demo">',1)
s=s.replace('C64 OS','C64OS').replace('C64 os','C64OS')
page.write_text(s)
print('Added video and',len(r['stages']),'screenshots; earlier demos retained')
