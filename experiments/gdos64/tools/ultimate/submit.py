"""Queue a structured command for the Terminal-owned hardware worker."""
from pathlib import Path
import json,time,sys
p=(Path(__file__).resolve().parents[4].parent/'gdos64-bridge')/'evidence/ultimate/jobs'/f'{time.time_ns()}.json'
p.write_text(sys.argv[1]);r=p.with_suffix('.result');deadline=time.monotonic()+90
while not r.exists():
 if time.monotonic()>deadline:raise SystemExit('Pending job: '+str(p))
 time.sleep(.2)
print(r.read_text())
if not json.loads(r.read_text())['ok']:raise SystemExit(1)
