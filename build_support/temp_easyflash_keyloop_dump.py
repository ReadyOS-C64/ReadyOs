#!/usr/bin/env python3
"""Capture and verify ReadyOS EasyFlash launcher key-loop runtime state."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OBJ = ROOT / "obj"
BIN = ROOT / "bin"
OUT = OBJ / "easyflash_keyloop_dump"
CRT = ROOT / "releases/0.2/precog-easyflash/readyos_easyflash.crt"
D64 = ROOT / "releases/0.2/precog-easyflash/readyos_data.d64"
LAYOUT = OBJ / "easyflash_layout.json"
LAUNCHER_MAP = OBJ / "launcher_easyflash.map"
BOOT_MAP = OBJ / "boot_easyflash_roml.map"


def fail(msg: str) -> None:
    raise RuntimeError(msg)


def map_symbol(path: Path, symbol: str) -> int:
    text = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(rf"(?:^|\s){re.escape(symbol)}\s+([0-9A-Fa-f]{{6}})\s+RLA", text)
    if not m:
        fail(f"symbol {symbol} not found in {path}")
    return int(m.group(1), 16) & 0xFFFF


def map_segment_end(path: Path, name: str) -> int:
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split()
        if len(parts) >= 4 and parts[0] == name and re.fullmatch(r"[0-9A-Fa-f]{6}", parts[1]):
            return int(parts[2], 16) & 0xFFFF
    fail(f"segment {name} not found in {path}")


def parse_monitor_log(path: Path) -> dict[int, bytes]:
    dumps: dict[int, bytes] = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"^>C:([0-9a-fA-F]{4})\s+(.*)$", line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        values: list[int] = []
        for token in m.group(2).split():
            if not re.fullmatch(r"[0-9a-fA-F]{2}", token):
                break
            values.append(int(token, 16))
        if values:
            dumps[addr] = bytes(values)
    return dumps


def region_bytes(dumps: dict[int, bytes], start: int, length: int) -> bytes:
    out = bytearray()
    cursor = start
    while len(out) < length:
        chunk = dumps.get(cursor)
        if not chunk:
            break
        out.extend(chunk)
        cursor += len(chunk)
    return bytes(out[:length])


def prg_payload(path: Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) < 3:
        fail(f"short PRG: {path}")
    return raw[2:]


def snapshot_modules(path: Path) -> dict[str, list[tuple[int, bytes]]]:
    data = path.read_bytes()
    pos = 0x3A
    modules: dict[str, list[tuple[int, bytes]]] = {}
    while pos + 22 <= len(data):
        name = data[pos:pos + 16].split(b"\0", 1)[0].decode("ascii", "replace")
        length = int.from_bytes(data[pos + 18:pos + 22], "little")
        if not name or length < 22 or pos + length > len(data):
            break
        modules.setdefault(name, []).append((pos, data[pos + 22:pos + length]))
        pos += length
    return modules


def extract_reu(snapshot_path: Path) -> bytes:
    mods = snapshot_modules(snapshot_path)
    entries = mods.get("REU1764")
    if not entries:
        fail("snapshot has no REU1764 module")
    data = entries[0][1]
    if len(data) < 20 + 16 * 1024 * 1024:
        fail(f"REU1764 module is too short: {len(data)} bytes")
    return data[20:20 + 16 * 1024 * 1024]


def compare_at(label: str, actual: bytes, expected: bytes, failures: list[str]) -> None:
    if actual[:len(expected)] == expected:
        return
    limit = min(len(actual), len(expected))
    mismatch = next((i for i in range(limit) if actual[i] != expected[i]), limit)
    failures.append(
        f"{label}: mismatch at +${mismatch:04X}, "
        f"expected ${expected[mismatch]:02X}, saw ${actual[mismatch]:02X}"
    )


def write_monitor(mon: Path, snapshot: Path, ram: Path, io: Path) -> None:
    preload = map_symbol(BOOT_MAP, "easyflash_preload_verify_done")
    keyloop = map_symbol(LAUNCHER_MAP, "_tui_getkey")
    lines = [
        f"break {preload:04x}",
        f"break {keyloop:04x}",
        "x",
        "m ca00 cb1f",
        "m c600 c9ff",
        "m de00 de02",
        "m df00 df08",
        "x",
        "m 0000 003f",
        "m 0314 0319",
        "m 0400 04ff",
        "m c600 c9ff",
        "m c7a0 c7ef",
        "m c836 c839",
        "m d011 d012",
        "m d016 d018",
        "m d020 d021",
        "m dd00 dd02",
        "m dc00 dc0f",
        "m dd00 dd0f",
        "m de00 de02",
        "m df00 df08",
        "bank ram",
        f"save \"{ram}\" 0 0000 ffff",
        "bank io",
        f"save \"{io}\" 0 d000 dfff",
        "bank ram",
        f"dump \"{snapshot}\"",
        "q",
    ]
    mon.write_text("\n".join(lines) + "\n", encoding="ascii")


def strip_loadaddr(path: Path) -> bytes:
    raw = path.read_bytes()
    if len(raw) >= 2:
        return raw[2:]
    return b""


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    runtime_crt = OUT / "readyos_easyflash.runtime.crt"
    mon = OUT / "keyloop_dump.mon"
    log = OUT / "keyloop_dump.log"
    ram = OUT / "c64_ram.prg"
    io = OUT / "io_d000_dfff.prg"
    snapshot = OUT / "keyloop.vsf"
    report = OUT / "report.json"

    for required in (CRT, D64, LAYOUT, LAUNCHER_MAP, BOOT_MAP):
        if not required.exists():
            fail(f"missing required artifact: {required}")
    shutil.copy2(CRT, runtime_crt)
    write_monitor(mon, snapshot, ram, io)
    log.unlink(missing_ok=True)
    ram.unlink(missing_ok=True)
    io.unlink(missing_ok=True)
    snapshot.unlink(missing_ok=True)

    cmd = [
        "x64sc", "-console", "-default", "+sound", "-warp",
        "-reu", "-reusize", "16384",
        "-cartcrt", str(runtime_crt),
        "-drive8type", "1541", "-devicebackend8", "0", "+busdevice8",
        "-8", str(D64),
        "-moncommands", str(mon),
        "-monlog", "-monlogname", str(log),
        "-initbreak", "0xe000",
        "-limitcycles", "120000000",
    ]
    subprocess.run(cmd, cwd=ROOT, check=False)

    if not log.exists():
        fail("missing monitor log")
    if not ram.exists():
        fail("missing C64 RAM save")
    if not io.exists():
        fail("missing I/O save")
    if not snapshot.exists():
        fail("missing VICE snapshot dump")

    layout = json.loads(LAYOUT.read_text(encoding="utf-8"))
    dumps = parse_monitor_log(log)
    ram_bytes = strip_loadaddr(ram)
    io_bytes = strip_loadaddr(io)
    reu = extract_reu(snapshot)
    failures: list[str] = []

    launcher_payload = prg_payload(BIN / "launcher_easyflash.prg")
    rodata_end = map_segment_end(LAUNCHER_MAP, "RODATA")
    immutable_launcher_len = rodata_end - 0x1000 + 1
    compare_at("launcher REU bank 0", reu[0:0x10000], launcher_payload, failures)
    compare_at(
        "launcher immutable RAM $1000..RODATA",
        ram_bytes[0x1000:0x1000 + immutable_launcher_len],
        launcher_payload[:immutable_launcher_len],
        failures,
    )
    for app in layout["apps"]:
        bank = int(app["bank"])
        artifact = ROOT / str(app["artifact"])
        compare_at(f"app {app['prg']} REU bank {bank}", reu[bank * 0x10000:(bank + 1) * 0x10000], prg_payload(artifact), failures)
    for overlay in layout["overlays"]:
        bank = int(overlay["reu_bank"])
        off = int(overlay["reu_off"])
        artifact = ROOT / str(overlay["artifact"])
        start = bank * 0x10000 + off
        compare_at(f"overlay {overlay['name']} REU ${start:06X}", reu[start:start + int(overlay["payload_len"])], prg_payload(artifact), failures)

    diag = region_bytes(dumps, 0xCA00, 0x120)
    cart_regs = region_bytes(dumps, 0xDE00, 3)
    reu_regs = region_bytes(dumps, 0xDF00, 9)
    zpage = region_bytes(dumps, 0x0000, 0x40)
    cia1 = region_bytes(dumps, 0xDC00, 0x10)
    cia2 = region_bytes(dumps, 0xDD00, 0x10)
    irq_vec = region_bytes(dumps, 0x0314, 6)
    bitmap = region_bytes(dumps, 0xC836, 4)
    key_editor = ram_bytes[0x0286:0x0290] if len(ram_bytes) >= 0x0290 else b""

    if len(diag) < 0x30 or diag[:4] != b"EFV\x01" or diag[5] != 0:
        failures.append(f"preload diagnostics not clean: {diag[:8].hex(' ')}")
    if len(cart_regs) >= 3 and cart_regs[2] != 0x04:
        failures.append(f"EasyFlash control is not ROM-off at key loop: DE02=${cart_regs[2]:02X}")
    if len(bitmap) >= 4 and bitmap != bytes([0xFE, 0xFF, 0x01, 0x08]):
        failures.append(f"unexpected preload bitmap/storage drive: {bitmap.hex(' ')}")
    if len(irq_vec) >= 6 and irq_vec[:6] != bytes([0x31, 0xEA, 0x66, 0xFE, 0x47, 0xFE]):
        failures.append(f"KERNAL IRQ/BRK/NMI vectors are not restored: {irq_vec.hex(' ')}")
    expected_key_editor = bytes([0x0E, 0x0E, 0x04, 0x0A, 0xFF, 0x04, 0x0A, 0x00, 0x00, 0x48])
    if len(key_editor) >= len(expected_key_editor) and key_editor[:len(expected_key_editor)] != expected_key_editor:
        failures.append(
            "KERNAL editor/keyboard variables at $0286-$028F are not restored: "
            f"{key_editor[:len(expected_key_editor)].hex(' ')}"
        )

    summary = {
        "ok": not failures,
        "failures": failures,
        "artifacts": {
            "monitor_log": str(log.relative_to(ROOT)),
            "c64_ram": str(ram.relative_to(ROOT)),
            "io": str(io.relative_to(ROOT)),
            "snapshot": str(snapshot.relative_to(ROOT)),
            "report": str(report.relative_to(ROOT)),
        },
        "state": {
            "zpage_0000_003f": zpage.hex(" "),
            "cpu_port_0001": zpage[1] if len(zpage) > 1 else None,
            "irq_vectors_0314_0319": irq_vec.hex(" "),
            "key_editor_0286_028f": key_editor.hex(" "),
            "cart_de00_de02": cart_regs.hex(" "),
            "reu_df00_df08": reu_regs.hex(" "),
            "cia1_dc00_dc0f": cia1.hex(" "),
            "cia2_dd00_dd0f": cia2.hex(" "),
            "bitmap_c836_c839": bitmap.hex(" "),
            "diag_header_ca00": diag[:8].hex(" "),
            "screen_prefix_0400": region_bytes(dumps, 0x0400, 0x40).hex(" "),
        },
        "counts": {
            "apps_verified_from_reu": len(layout["apps"]),
            "overlays_verified_from_reu": len(layout["overlays"]),
            "reu_snapshot_bytes": len(reu),
            "c64_ram_bytes": len(ram_bytes),
            "io_bytes": len(io_bytes),
        },
    }
    report.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"report: {report}")
    print(f"C64 RAM dump: {ram}")
    print(f"I/O dump: {io}")
    print(f"VICE snapshot: {snapshot}")
    print(f"cart $DE00-$DE02: {cart_regs.hex(' ')}")
    print(f"REU $DF00-$DF08: {reu_regs.hex(' ')}")
    print(f"CPU $0001: {summary['state']['cpu_port_0001']}")
    print(f"CIA1 $DC00-$DC0F: {cia1.hex(' ')}")
    print(f"bitmap $C836-$C839: {bitmap.hex(' ')}")
    if failures:
        print("FAIL:")
        for item in failures:
            print(f"  - {item}")
        return 1
    print("easyflash key-loop RAM/REU dump verification passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
