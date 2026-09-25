#!/usr/bin/env python3
"""Validate disk-only demos; optionally execute their workers with py65 (no VICE)."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile

from build_readybasic_disk_modules import sample_inventory
import readyos_profiles
from verify_readybasic_plugin import token_unsafe_reason

ROOT = Path(__file__).resolve().parents[1]
MODULES = ROOT / "obj/readybasic_modules"


def word(data, offset):
    return int.from_bytes(bytes(data[offset:offset + 2]), "little")


def read_package(name):
    data = (MODULES / f"rbm.{name}.seq").read_bytes()
    assert data[:5] == b"RBM!\x01", name
    descriptors = [data[16 + i * 32:48 + i * 32] for i in range(data[6])]
    payloads = {}
    pos = 16 + 32 * data[6]
    for _ in range(data[7]):
        offset, size = word(data, pos), word(data, pos + 2)
        assert offset not in payloads
        payloads[offset] = data[pos + 6:pos + 6 + size]
        assert len(payloads[offset]) == size
        pos += 6 + size
    assert pos == len(data), (name, "trailing or short records")
    return word(data, 8), descriptors, payloads


def command_name(desc):
    return desc[16:16 + desc[15]].decode("ascii")


def check_packages():
    source = (ROOT / "src/apps/readybasic/readybasic.s").read_text()
    assert not re.search(r"^cmd_(?:echo1|add16|hiddenram|sumnumarray|rangenumarray|tempscratch|fail|slot[012]|span|ovl[12]|cpyrst|copy)(?:_\w+)?:", source, re.M)
    prg = (ROOT / "bin/readybasic.prg").read_bytes()
    seed = 2 + 0x5000 - 0x1000 + 16
    registry = prg[seed:seed + 4096]
    cold = [registry[i:i + 32] for i in range(0, 4096, 32) if registry[i + 15]]
    names = {command_name(d) for d in cold}
    assert len(names) == 83 and {"PAUSE", "LDMOD"} <= names
    assert not any(n.startswith("Z") for n in names)
    assert not names.intersection(n for group in sample_inventory().values() for n in group)
    assert command_name(cold[-1]) == "SCRPUT" and registry[-17] == 6
    all_ids = {d[0] for d in cold}
    production_submodules = {d[6] for d in cold}
    media_off, media, _ = read_package("media")
    assert media_off == 0x1A40 and len(media) == 8
    all_ids.update(d[0] for d in media)
    production_submodules.update(d[6] for d in media)
    result = {}
    for package, expected in sample_inventory().items():
        offset, descriptors, payloads = read_package(package)
        assert [command_name(d) for d in descriptors] == expected
        assert 1 <= len(descriptors) <= 32 and len(payloads) <= 32
        assert 0x1B40 <= offset < offset + len(descriptors) * 32 <= 0x1F40
        assert not any(registry[offset - 0x1000:offset - 0x1000 + len(descriptors) * 32])
        for d in descriptors:
            name = command_name(d)
            assert not name.startswith("Z") and token_unsafe_reason(name + "(") is None, name
            assert d[0] not in all_ids and d[6] not in production_submodules, name
            payload = payloads[word(d, 2)]
            assert len(payload) == word(d, 4) and word(d, 12) < len(payload)
            assert d[8] in (1, 2, 4, 6) and word(d, 10) == 0
            assert len(payload) <= (4096 if d[8] == 6 else 2048)
            assert 0xA000 <= word(d, 2) < word(d, 2) + len(payload) <= 0xCF00
        result[package] = (offset, descriptors, payloads)
    a, b, c = (result[n] for n in ("sample1", "sample2", "sample3"))
    assert a[0] + len(a[1]) * 32 == b[0]
    assert c[0] == a[0] and c[0] + len(c[1]) * 32 >= b[0] + len(b[1]) * 32
    # Replacing the scalar samples with sample3 must leave no stale sample1/2 names.
    active = bytearray(registry)
    for offset, descriptors, _ in (a, b, c):
        active[offset - 0x1000:offset - 0x1000 + 32 * len(descriptors)] = b"".join(descriptors)
    active_names = {command_name(active[i:i + 32]) for i in range(0, 4096, 32) if active[i + 15]}
    assert names <= active_names
    assert not (set(sample_inventory()["sample1"] + sample_inventory()["sample2"]) - {"COPY", "CPYRST"}) & active_names
    # Keep the compact 13-block sample budget after removing duplicate workers,
    # without removing descriptors or overlay examples.
    sample_blocks = sum(((MODULES / f"rbm.{name}.seq").stat().st_size + 253) // 254
                        for name in sample_inventory())
    assert sample_blocks <= 13, ("Ultimate sample disk budget", sample_blocks)
    for profile_id in readyos_profiles.list_profile_ids():
        disks = readyos_profiles.load_profile(profile_id)["disks"]
        if any(e["name"] in ("rbtest1", "rbproc1") for d in disks for e in d["contents"]):
            assert any(e["name"] == "rbm.sample1" for d in disks
                       if d["drive"] == 8 and d.get("mount_on_boot", True)
                       for e in d["contents"]), (profile_id, "sample1 must be on loader drive 8")
    print("Sample packages OK: 83 built-ins preserved; 12/7/32 descriptors; slot, overlay and media ranges disjoint")
    return result


def check_workers(packages):
    from py65.devices.mpu6502 import MPU

    symbols = {line.split()[2].lstrip("."): int(line.split()[1], 16)
               for line in (ROOT / "obj/readybasic.labels").read_text().splitlines()
               if line.startswith("al ")}
    mpu = MPU()
    memory = mpu.memory
    heap = bytearray(256)
    workers = {}
    for _, descriptors, payloads in packages.values():
        for d in descriptors:
            workers[command_name(d)] = (d, payloads[word(d, 2)])

    def putword(address, value):
        memory[address:address + 2] = [value & 255, (value >> 8) & 255]

    def call(name, reload=True):
        desc, payload = workers[name]
        base = {1: 0xA800, 2: 0xB000, 4: 0xB800, 6: 0xB000}[desc[8]]
        if reload:
            memory[base:base + len(payload)] = payload
        memory[0xC300:0xC400] = [0] * 256
        mpu.sp = 0xFF
        mpu.stPushWord(0x01FF)  # RTS returns to the host sentinel at $0200.
        mpu.pc = base + word(desc, 12)
        for _ in range(200000):
            if mpu.pc == 0x0200:
                return word(memory, 0xC303)
            if mpu.pc == symbols["rb_reu_fetch"]:
                assert memory[symbols["rb_reu_off_hi"]] == 0x0C
                assert memory[symbols["rb_reu_len_hi"]] == 1
                memory[0xC500:0xC600] = heap
                mpu.pc = mpu.stPopWord() + 1
            else:
                mpu.step()
        raise AssertionError(f"worker hung: {name}")

    assert call("ECHO1") == 1
    putword(0xC210, 7); putword(0xC212, 8)
    assert call("ADD16") == 15
    putword(0xC210, 0xFFFF); putword(0xC212, 2)
    assert call("ADD16") == 1
    memory[0xC250] = 2; memory[0xC260:0xC262] = b"aB"
    assert call("HIDDENRAM") == 131
    putword(0xC240, 0x4000); memory[0xC242] = 3
    memory[0x4000:0x4006] = [0, 1, 0xFF, 0xFE, 0, 6]
    assert call("SUMNUMARRAY") == 5
    putword(0xC210, 7); putword(0xC212, 4)
    call("RANGENUMARRAY")
    assert memory[0xC380:0xC388] == [0, 7, 0, 8, 0, 9, 0, 10]
    assert memory[0xC302] == 3 and word(memory, 0xC305) == 4
    putword(0xC210, 513)
    assert call("TEMPSCRATCH") == 3 and not any(memory[0xC500:0xC5C0])
    assert not any(heap)
    heap[:192] = bytes([1]) * 192
    call("TEMPSCRATCH")
    assert memory[0xC300] == 0x26 and memory[0xC301] == 0x26
    putword(0xC210, 7)
    call("FAIL")
    assert memory[0xC300] == 7 and memory[0xC301] == 7
    putword(0xC210, 0)
    call("FAIL")
    assert memory[0xC300] == 127
    for name, expected in {"SLOT0": 30, "SLOT1": 31, "SLOT2": 32, "SPAN": 40,
                           "OVL1": 51, "OVL2": 52, "DM1": 61, "DM2S": 74,
                           "DOV1": 72, "DOV2": 73}.items():
        assert call(name) == expected, name
    memory[symbols["rb_copy_count"]] = 9
    assert call("COPY") == 9
    assert call("CPYRST") == 0 and call("COPY") == 0
    for prefix, base in (("S6", 0), ("S7", 50), ("S8", 100)):
        for index, overlay in enumerate("ABCDE", 1):
            a, b = prefix + overlay + "A", prefix + overlay + "B"
            assert call(a) == base + index * 2 + 2
            assert call(b, reload=False) == base + index * 2 + 4
            assert call(a) == base + index * 2 + 2
            for count in range(2, 261):
                name = b if count % 2 == 0 else a
                expected_base = base + index * 2 + (2 if name == b else 1)
                assert call(name, reload=False) == expected_base + (count & 255), (name, count)
                assert memory[0xC300] == 0 and memory[0xC302] == 1
    print("6502 worker checks OK: arithmetic, strings, arrays, scratch success/failure, error codes, every slot/overlay and shared state")


def check_decimal_display():
    """Execute the actual formatter; include zeroes after significant digits."""
    from py65.devices.mpu6502 import MPU
    source = (ROOT / 'src/apps/readybasic/readybasic.s').read_text()
    routine = source.split('hidden_calc_basic_free:', 1)[1].split('hidden_prepare_ready_resume:', 1)[0]
    symbols = ['rb_free_lo', 'rb_free_hi', 'rb_tmp_lo', 'rb_digit_seen', 'rb_digit_count']
    assembly = '\n'.join(f'{name} = ${0x3000 + i:04x}' for i, name in enumerate(symbols))
    assembly += '\nFRETOP=$33\nSTREND=$31\nK_CHROUT=$FFD2\n.segment "CODE"\n'
    assembly += 'jsr hidden_print_live_free\nbrk\nhidden_calc_basic_free:' + routine
    with tempfile.TemporaryDirectory(prefix='readybasic-decimal-') as directory:
        path = Path(directory)
        (path / 'test.s').write_text(assembly)
        (path / 'test.cfg').write_text('MEMORY { M: start=$1000, size=$1000, file=%O; } SEGMENTS { CODE: load=M, type=ro; }')
        subprocess.run(['ca65', str(path / 'test.s'), '-o', str(path / 'test.o')], check=True)
        subprocess.run(['ld65', '-C', str(path / 'test.cfg'), str(path / 'test.o'), '-o', str(path / 'test.bin')], check=True)
        code = (path / 'test.bin').read_bytes()
    for value in (0, 1, 10, 100, 101, 1000, 10001, 30013, 40960, 65535):
        cpu = MPU()
        cpu.memory[0x1000:0x1000 + len(code)] = code
        cpu.memory[0x33:0x35] = [255, 255]
        cpu.memory[0x31:0x33] = list((65535 - value).to_bytes(2, 'little'))
        cpu.memory[0xffd2] = 0x60  # CHROUT returns; capture A at this boundary.
        cpu.pc = 0x1000
        printed = ''
        for _ in range(10000):
            if cpu.pc == 0x1003:
                break
            if cpu.pc == 0xffd2:
                printed += chr(cpu.a)
            cpu.step()
        else:
            raise AssertionError('decimal formatter did not return')
        assert printed == str(value), (value, printed)
    print('6502 decimal display checks OK: leading/internal/trailing zeroes, cold free bytes, 16-bit limits')


def check_probe_load_order():
    # ECHO1 is no longer resident. Both the generator and generated plan must
    # load sample1 before their first direct health check, including cold boots.
    for name in ('run_readybasic_full_suite_visual_verification.sh',
                 'readybasic_full_suite_visual_verification.generated.yaml'):
        text = (ROOT / 'build_support' / name).read_text()
        assert text.index('  - id: load_disk_demo_commands\n') < text.index('  - id: direct_ping\n'), name
        assert 'direct_old_add16_rejected' not in text, 'ADD16 is now a valid public command'
        assert 'direct_old_zadd16_rejected' in text, 'Reject the retired ZADD16 spelling instead'
    print('Cold full-suite demo dependency order OK')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workers", action="store_true", help="execute workers using optional py65")
    args = parser.parse_args()
    packages = check_packages()
    check_probe_load_order()
    if args.workers:
        check_workers(packages)
        check_decimal_display()
