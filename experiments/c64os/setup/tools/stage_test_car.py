import sys,os,socket,subprocess,time
from pathlib import Path
root=Path(__file__).resolve().parents[4]
sys.path.insert(0,str(root/'experiments/c64os'))
from cap_native_reu import Monitor,feed,wait_bytes
from verify_bridge_ui import screen,wait_screen
env=root.parent/'c64os-readyos-experiment/bridge-setup-test-20260926'
subprocess.run(['ca65','-o',str(env/'install_file.o'),str(root/'experiments/c64os/install_file.s')],check=True)
(env/'install_file.cfg').write_text('MEMORY { RAM: start=$C000, size=$0200, file=%O; }\nSEGMENTS { CODE: load=RAM, type=rw; }\n')
subprocess.run(['ld65','-C',str(env/'install_file.cfg'),'-o',str(env/'install_file.bin'),str(env/'install_file.o')],check=True)
with socket.create_connection(('127.0.0.1',6514),5) as sock:
 sock.settimeout(30);m=Monitor(sock)
 resource=b'IDE64Image1';res=m.command(0x51,bytes([len(resource)])+resource)
 assert Path(res[2:2+res[1]].rstrip(b'\0').decode()).resolve()==(env/'runtime/vice support/c64os v1.09.hdd').resolve(), 'Not the owned test HDD'
 m.command(0xcc,b'\x01');m.command(0xaa);wait_screen(m,'COMMODORE 64 BASIC V2')
 name=b'@1//:BRIDGE.CAR,S,W';data=(root/'assets/c64os/bridge.car').read_bytes()
 m.write(0xc000,(env/'install_file.bin').read_bytes());m.write(0xc200,name);m.write(0x2000,data)
 m.write(0xc2f0,bytes([len(name)])+len(data).to_bytes(2,'little')+b'\0');feed(m,b'SYS49152\r');wait_bytes(m,0xc2f3,b'\1');m.command(0xaa);time.sleep(.3)
 feed(m,b'OPEN15,12,15,"CD//OS":CLOSE15\r');time.sleep(.5);feed(m,b'LOAD"BOOTER",12\r');time.sleep(1);feed(m,b'RUN\r');print(wait_screen(m,'DEVICES',60));m.command(0xaa)
