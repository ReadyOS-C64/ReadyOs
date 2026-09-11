#!/usr/bin/env python3
"""Read-only resource layout and tokenized native-identifier checks."""
from pathlib import Path
import hashlib
import json
import re

ROOT=Path(__file__).resolve().parents[1]
def main():
    assets=ROOT/"assets/readybasic/neon"
    hashes=json.loads((assets/"checksums.json").read_text())
    for name,digest in hashes.items():
        assert hashlib.sha256((assets/name).read_bytes()).hexdigest()==digest,name
    koa=(assets/"rb.neon.koa").read_bytes()
    assert len(koa)==10003 and koa[:2]==b"\0\x60"
    assert all(c<16 for c in koa[9002:])
    spr=(assets/"rb.ready.rbr").read_bytes()
    assert len(spr)==328 and spr[:8]==b"RBR1\x00\xca\x40\x01"
    assert all(spr[8+i*64+63]==0 for i in range(5))
    for stem in ('rbgfxsnddemo', 'rbugfxsnddemo'):
        check_demo(stem)
    print("NEON STATIC OK: both demos, Koala 10003 bytes, five aligned sprites; native names stay ASCII")

def check_demo(stem):
    prg=(ROOT/f"obj/{stem}.prg").read_bytes()
    assert prg[:2]==b"\xc1\x2a"
    lines={}; pos=2
    while prg[pos:pos+2]!=b"\0\0":
        number=int.from_bytes(prg[pos+2:pos+4],"little")
        end=prg.index(0,pos+4)
        lines[number]=prg[pos+4:end]
        pos=end+1
    sources=(ROOT/f"src/apps/readybasic/{stem}.bas").read_text().splitlines()
    for source in sources:
        if not source.strip():continue
        number,text=source.split(maxsplit=1)
        if text.lstrip().startswith("rem "):continue
        for kind,name in re.findall(r"\b(exec|proc|func)\s+([a-z]+)",text):
            assert f"{kind} {name}".upper().encode() in lines[int(number)], \
                f"line {number}: ROM token in native name {name}"
    asm=(ROOT/"src/apps/readybasic/readybasic.s").read_text()
    known=set(re.findall(r'^\s+CMD_\w+\s+CMD_[^\n]*,\s*"([A-Z0-9]+)"',asm,re.M))
    module=(ROOT/"obj/readybasic_modules/rbm.media.seq").read_bytes()
    for i in range(module[6]):
        desc=module[16+i*32:48+i*32]
        known.add(desc[16:16+desc[15]].decode())
    known.update(("INT","SIN","PEEK","FRE","ELAPSED","PHASE"))
    for source in sources:
        if not source.strip():continue
        number,text=source.split(maxsplit=1)
        if text.lstrip().startswith(("rem ","proc ","func ")):continue
        for name in re.findall(r'\b([a-z]+)\(',text):
            assert name.upper() in known,f"line {number}: unknown command/function {name}"
    print(f"{stem}: {len(prg)}-byte BASIC")

if __name__=="__main__": main()
