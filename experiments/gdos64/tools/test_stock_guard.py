"""Run against a fresh stock skip-zero disk, never the experimental disk."""
from probe import *
import json
wait_ready('APPLICATIONS:')
with monitor() as m:
    assert m.read(0xc83b,1)==b'\0', 'This must be the stock skip-zero boot'

def open_settings():
    feed(bytes([19,17,17,13]))
    wait_ready('GDOS64 BRIDGE SETUP REQUIRED')
    wait_ready('THIS READYOS BOOT SKIPS 0 BANKS')
    screenshot(ROOT/'evidence/stock-guard.png')
    feed('L')
    wait_ready('GDOS64 BOOT LOCATION')

open_settings()
feed('1')
time.sleep(.3)
feed(bytes([20])+b'8\r')
wait_ready('EDITS APPLY NOW')
feed('S')
wait_ready('SAVED AND VERIFIED ON DEVICE 8')
screenshot(ROOT/'evidence/config-saved.png')
feed('\r')
wait_ready('APPLICATIONS:')
open_settings()
s=wait_ready('SAVED LOCATION LOADED')
assert s.splitlines()[5].strip()=='8',s
# Leave the private disk with the default GDOS boot device again.
feed('1');time.sleep(.3);feed(bytes([20])+b'9\r')
wait_ready('EDITS APPLY NOW');feed('S');wait_ready('SAVED AND VERIFIED ON DEVICE 8')
feed('\r');wait_ready('APPLICATIONS:')
(ROOT/'evidence/stock-guard-report.json').write_text(json.dumps({
    'pass':True,'checks':['stock skip-zero refuses transition',
    'boot device edited and saved with readback',
    'saved device reloaded after exiting and reopening bridge',
    'default device restored to 9']},indent=2)+'\n')
print('PASS stock guard and persistent location configuration')
