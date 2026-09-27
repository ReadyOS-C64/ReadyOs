#!/usr/bin/env python3
"""Exercise editable device-8 config through the real ReadyOS launcher/app."""
import json,socket
from verify_bridge_ui import ENV,BINARY_PORT,Monitor,feed,wait_screen
from verify_roundtrip_ui import launcher_app
with socket.create_connection(('127.0.0.1',BINARY_PORT),5) as sock:
 sock.settimeout(20);m=Monitor(sock)
 launcher_app(m,0);wait_screen(m,'START / RESUME C64OS')
 feed(m,b'L');wait_screen(m,'C64OS BOOT LOCATION')
 assert '12' in wait_screen(m,'BOOT FILENAME')
 feed(m,b'4'+bytes([20])*16+b'COOTER\rS')
 wait_screen(m,'SAVED AND VERIFIED ON DEVICE 8')
 feed(m,b'\rC');wait_screen(m,'COLD BOOT FAILED: 4; READYOS RESTORED')
 feed(m,b'L');wait_screen(m,'C64OS BOOT LOCATION')
 feed(m,b'4'+bytes([20])*16+b'BOOTER\rS')
 wait_screen(m,'SAVED AND VERIFIED ON DEVICE 8')
 feed(m,b'\r');wait_screen(m,'START / RESUME C64OS')
 # Leave the owned runtime disk flushed, then restart normally to verify reload.
 m.command(0xbb)
(ENV/'boot-config-ui.json').write_text(json.dumps({'pass':True,'edited_and_saved_on_device':8,'custom_filename_used_by_cold_loader':True,'failed_load_restored_readyos':True,'booter_location_restored_and_saved':True},indent=2)+'\n')
print('PASS: device-8 saved config drives the real cold loader; invalid target recovers')
