#!/usr/bin/env python3
"""Read-only checks of the built MEMCAP/media ABI and licensed PSID fixture."""
from pathlib import Path
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[1]
u16 = lambda data, offset: struct.unpack_from("<H", data, offset)[0]
be16 = lambda data, offset: struct.unpack_from(">H", data, offset)[0]

def main():
    module = (ROOT / "obj/readybasic_modules/rbm.media.seq").read_bytes()
    assert module[:8] == b"RBM!\x01\x06\x05\x01"
    assert u16(module, 8) == 0x1be0
    names = ["MUSTUNE", "MUSPLAY", "MUSHALT", "MUSDROP", "RSCFILE"]
    signatures = [19, 10, 24, 24, 19]
    record = 16 + 5 * 32
    assert u16(module, record) == 0x8000
    size = u16(module, record+2)
    assert record + 6 + size == len(module)
    assert size <= 0x1000
    for i, (name, signature) in enumerate(zip(names, signatures)):
        desc = module[16+i*32:48+i*32]
        assert desc[:2] == bytes([110+i, 6])
        assert u16(desc, 2) == 0x8000 and u16(desc, 4) == size
        assert desc[6:10] == bytes([24, 0, 6, 1])
        assert u16(desc, 10) == 0 and u16(desc, 12) < size
        assert desc[14:16] == bytes([signature, len(name)])
        assert desc[16:16+len(name)] == name.encode()

    prg = (ROOT / "bin/readybasic.prg").read_bytes()
    assert u16(prg, 0) == 0x1000 and len(prg) == 28674
    offset = 2 + 0x5000 - 0x1000 + 16 + 93*32
    cap = prg[offset:offset+32]
    assert cap[:2] == bytes([109, 1])
    # INPUTEV, not proof overlay 5 (which collides with rbm.sample2).
    assert cap[6:9] == bytes([19, 2, 4])
    assert cap[14:22] == b"\x0a\x06MEMCAP"
    border = prg[offset+32:offset+64]
    assert border[:2] == bytes([115, 3])  # Graphics; media owns IDs 110-114.
    assert border[14:22] == b"\x0a\x06BORDER"
    assert border[8] == 2  # GFXCORE in slot 1, not resident parser code.
    media_registry = 2 + 0x5000 - 0x1000 + 16 + u16(module, 8) - 0x1000
    assert prg[media_registry:media_registry+5*32] == bytes(5*32), \
        "media registration must not replace built-in commands"
    demo = (ROOT / "src/apps/readybasic/rbsnd07_psid.bas").read_bytes()
    assert b"border(c%)" in demo and b"ret% (ix%+1) and 15" in demo
    assert b"rscfile" not in demo and b"rb.colors" not in demo

    labels = {}
    for line in (ROOT / "obj/readybasic_modules/media.labels").read_text().splitlines():
        _, address, name = line.split()
        labels[name.lstrip(".")] = int(address, 16)
    assert labels["__DRIVER_RUN__"] == 0x9000
    assert labels["__DRIVER_SIZE__"] < 256
    assert 0x9000 <= labels["driver_start"] < 0x9100

    song = (ROOT / "assets/readybasic/music/summer-9200.sid").read_bytes()
    assert hashlib.sha256(song).hexdigest() == "aa70c84615f8cadfae1edd1dc60a5250fd84c2cc412542059c509345544062aa"
    assert song[:4] == b"PSID" and be16(song, 4) == 2
    assert be16(song, 6) == 124 and song[18:22] == bytes(4)
    assert be16(song, 118) == 0x24  # PAL, 8580, no special flags.
    assert song[122:124] == bytes(2)
    load = be16(song, 8) or u16(song, 124)
    payload = song[124 + (2 if be16(song, 8) == 0 else 0):]
    assert load == 0x9200 and load+len(payload) == 0x9c41
    assert load <= be16(song, 10) < load+len(payload)
    assert load <= be16(song, 12) < load+len(payload)
    print(f"MEDIA ABI OK: {len(module)}-byte SEQ, {size}-byte payload, "
          f"{labels['__DRIVER_SIZE__']}-byte driver; MEMCAP SYSTEM/INPUTEV; "
          "licensed PAL PSID $9200-$9c40; cold PRG unchanged at 28674 bytes")

if __name__ == "__main__":
    main()
