#!/usr/bin/env python3
"""Build ReadyBASIC disk-loadable SEQ command module packages."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


SIG_SCRCAP = 14
SIG_BUFNEW = 8

RB_SLOT_PROOF_1 = 0x02
RB_SLOT_PROOF_2 = 0x04
RB_SLOT_PROOF_12 = RB_SLOT_PROOF_1 | RB_SLOT_PROOF_2
RB_SLOT1_BASE = 0xB000
RB_SLOT2_BASE = 0xB800


def int_command_payload(value: int) -> bytes:
    """Return assembler-equivalent 6502 code for a no-arg integer command."""
    return bytes(
        [
            0xA9,
            0x00,
            0x8D,
            0x00,
            0xC3,
            0xA9,
            0x01,
            0x8D,
            0x02,
            0xC3,
            0xA9,
            value & 0xFF,
            0x8D,
            0x03,
            0xC3,
            0xA9,
            0x00,
            0x8D,
            0x04,
            0xC3,
            0x60,
        ]
    )


def stateful_overlay_payload(value_a: int, value_b: int, runtime_base: int) -> tuple[bytes, int]:
    """Two entrypoints share result code and one overlay-local counter byte.

    Each entry supplies a 16-bit base in A/X. The common tail increments the
    byte counter, adds it with carry, and writes the standard integer result.
    """
    common_addr = runtime_base + 11
    state_addr = runtime_base + 38
    payload = bytes([
        0xA9, value_a & 0xFF, 0xA2, value_a >> 8,
        0x4C, common_addr & 0xFF, common_addr >> 8,
        0xA9, value_b & 0xFF, 0xA2, value_b >> 8,
        0xEE, state_addr & 0xFF, state_addr >> 8,  # INC state
        0x18,                                            # CLC
        0x6D, state_addr & 0xFF, state_addr >> 8,  # ADC state
        0x8D, 0x03, 0xC3,                        # STA RF_VAL_LO
        0x8A, 0x69, 0x00,                       # TXA / ADC #0
        0x8D, 0x04, 0xC3,                       # STA RF_VAL_HI
        0xA9, 0x00, 0x8D, 0x00, 0xC3,           # RF_STATUS = 0
        0xA9, 0x01, 0x8D, 0x02, 0xC3,           # RF_TAG = integer
        0x60, 0x00,                              # RTS / state byte
    ])
    assert len(payload) == 39
    return payload, 7


def num_string_payload(name: str) -> bytes:
    """Return 6502 code for NAME(n) -> NAME plus decimal n for 0..99."""
    name_bytes = name.encode("ascii")
    if len(name_bytes) > 15:
        raise ValueError(f"command name too long: {name}")
    code = bytearray(
        [
            0xA9,
            0x00,
            0x8D,
            0x00,
            0xC3,  # RF_STATUS = 0
            0xA9,
            0x02,
            0x8D,
            0x02,
            0xC3,  # RF_TAG = RB_VAL_STRING
        ]
    )
    for offset, value in enumerate(name_bytes):
        code.extend([0xA9, value, 0x8D, 0x20 + offset, 0xC3])
    tens_pos = 0x20 + len(name_bytes)
    ones_pos = tens_pos + 1
    len_one = len(name_bytes) + 1
    len_two = len(name_bytes) + 2
    code.extend(
        [
            0xAD,
            0x10,
            0xC2,  # lda CF_NUM0_LO
            0xA2,
            0x2F,  # ldx #'0' - 1
            0x38,  # sec
            0xE8,  # @tens: inx
            0xE9,
            0x0A,  # sbc #10
            0xB0,
            0xFB,  # bcs @tens
            0x69,
            0x0A,  # adc #10
            0xE0,
            0x30,  # cpx #'0'
            0xF0,
            0x0D,  # beq @one_digit
            0x8E,
            tens_pos,
            0xC3,  # stx RF_STR_BUF+len(name)
            0x18,  # clc
            0x69,
            0x30,  # adc #'0'
            0x8D,
            ones_pos,
            0xC3,  # sta RF_STR_BUF+len(name)+1
            0xA9,
            len_two,
            0xD0,
            0x08,  # bne @set_len
            0x18,  # @one_digit: clc
            0x69,
            0x30,  # adc #'0'
            0x8D,
            tens_pos,
            0xC3,  # sta RF_STR_BUF+len(name)
            0xA9,
            len_one,
            0x8D,
            0x10,
            0xC3,  # @set_len: sta RF_STR_LEN
            0x60,  # rts
        ]
    )
    return bytes(code)


def fixed_string_payload(text: str) -> bytes:
    """Return 6502 code for a string command that returns TEXT."""
    text_bytes = text.encode("ascii")
    if len(text_bytes) > 31:
        raise ValueError(f"return text too long: {text}")
    code = bytearray(
        [
            0xA9,
            0x00,
            0x8D,
            0x00,
            0xC3,  # RF_STATUS = 0
            0xA9,
            0x02,
            0x8D,
            0x02,
            0xC3,  # RF_TAG = RB_VAL_STRING
        ]
    )
    for offset, value in enumerate(text_bytes):
        code.extend([0xA9, value, 0x8D, 0x20 + offset, 0xC3])
    code.extend([0xA9, len(text_bytes), 0x8D, 0x10, 0xC3, 0x60])
    return bytes(code)


def descriptor(
    *,
    command_id: int,
    module_id: int,
    reu_offset: int,
    payload_size: int,
    submodule_id: int,
    overlay_id: int,
    slot_mask: int,
    generation: int,
    signature_id: int,
    name: str,
) -> bytes:
    name_bytes = name.encode("ascii")
    if len(name_bytes) > 15:
        raise ValueError(f"command name too long: {name}")
    return bytes(
        [
            command_id,
            module_id,
            reu_offset & 0xFF,
            reu_offset >> 8,
            payload_size & 0xFF,
            payload_size >> 8,
            submodule_id,
            overlay_id,
            slot_mask,
            generation,
            0,
            0,
            0,
            0,
            signature_id,
            len(name_bytes),
        ]
    ) + name_bytes.ljust(16, b"\x00")


def build_module(
    *,
    module_id: int,
    desc_reu_offset: int,
    commands: list[dict[str, int | str]],
) -> bytes:
    descriptors: list[bytes] = []
    payloads: list[tuple[int, bytes]] = []
    seen_payload_offsets: set[int] = set()
    for command in commands:
        if "payload" in command:
            payload = bytes(command["payload"])  # type: ignore[arg-type]
            payload_size = int(command["payload_size"])
            entry_offset = int(command["entry_offset"])
            signature_id = int(command.get("signature_id", SIG_BUFNEW))
        elif command.get("kind") == "num_string":
            payload = num_string_payload(str(command["name"]))
            payload_size = len(payload)
            entry_offset = 0
            signature_id = SIG_BUFNEW
        else:
            payload = int_command_payload(int(command["return_value"]))
            payload_size = len(payload)
            entry_offset = 0
            signature_id = SIG_SCRCAP
        reu_offset = int(command["reu_offset"])
        descriptors.append(
            descriptor(
                command_id=int(command["command_id"]),
                module_id=module_id,
                reu_offset=reu_offset,
                payload_size=payload_size,
                submodule_id=int(command["submodule_id"]),
                overlay_id=int(command["overlay_id"]),
                slot_mask=int(command["slot_mask"]),
                generation=1,
                signature_id=signature_id,
                name=str(command["name"]),
            )
        )
        if entry_offset:
            descriptor_bytes = bytearray(descriptors[-1])
            descriptor_bytes[12] = entry_offset & 0xFF
            descriptor_bytes[13] = entry_offset >> 8
            descriptors[-1] = bytes(descriptor_bytes)
        if reu_offset not in seen_payload_offsets:
            payloads.append((reu_offset, payload))
            seen_payload_offsets.add(reu_offset)

    desc_blob = b"".join(descriptors)
    if desc_reu_offset + len(desc_blob) > 0x2000:
        raise ValueError(
            f"module {module_id} descriptors overflow registry: "
            f"${desc_reu_offset:04x}+${len(desc_blob):04x}"
        )

    payload_blob = bytearray()
    for reu_offset, payload in payloads:
        if reu_offset + len(payload) > 0x10000:
            raise ValueError(
                f"module {module_id} payload overflows REU bank: "
                f"${reu_offset:04x}+${len(payload):04x}"
            )
        payload_blob.extend(
            [
                reu_offset & 0xFF,
                reu_offset >> 8,
                len(payload) & 0xFF,
                len(payload) >> 8,
                0,
                0,
            ]
        )
        payload_blob.extend(payload)

    blob = bytearray()
    blob.extend(b"RBM!")
    blob.extend(
        [
            1,
            module_id,
            len(descriptors),
            len(payloads),
            desc_reu_offset & 0xFF,
            desc_reu_offset >> 8,
            0,
            0,
            0,
            0,
            0,
            0,
        ]
    )
    blob.extend(desc_blob)
    blob.extend(payload_blob)
    return bytes(blob)


def rbm3_payload_commands() -> list[dict[str, int | str | bytes]]:
    commands: list[dict[str, int | str | bytes]] = []
    command_id = 147
    reu_offset = 0xC000
    for submodule_id, slot_mask in (
        (6, RB_SLOT_PROOF_2),
        (7, RB_SLOT_PROOF_2),
        (8, RB_SLOT_PROOF_12),
    ):
        submodule_name = {6: "S6", 7: "S7", 8: "S8"}[submodule_id]
        for overlay_id in range(1, 6):
            overlay_name = chr(ord("A") + overlay_id - 1)
            name_a = f"{submodule_name}{overlay_name}A"
            name_b = f"{submodule_name}{overlay_name}B"
            runtime_base = RB_SLOT2_BASE if slot_mask == RB_SLOT_PROOF_2 else RB_SLOT1_BASE
            payload, entry_b_offset = stateful_overlay_payload(
                (submodule_id - 6) * 50 + overlay_id * 2 + 1,
                (submodule_id - 6) * 50 + overlay_id * 2 + 2,
                runtime_base,
            )
            commands.append(
                {
                    "command_id": command_id,
                    "name": name_a,
                    "payload": payload,
                    "payload_size": len(payload),
                    "entry_offset": 0,
                    "reu_offset": reu_offset,
                    "submodule_id": submodule_id,
                    "overlay_id": overlay_id,
                    "slot_mask": slot_mask,
                    "signature_id": SIG_SCRCAP,
                }
            )
            command_id += 1
            commands.append(
                {
                    "command_id": command_id,
                    "name": name_b,
                    "payload": payload,
                    "payload_size": len(payload),
                    "entry_offset": entry_b_offset,
                    "reu_offset": reu_offset,
                    "submodule_id": submodule_id,
                    "overlay_id": overlay_id,
                    "slot_mask": slot_mask,
                    "signature_id": SIG_SCRCAP,
                }
            )
            command_id += 1
            reu_offset += 0x100
    return commands


def rbm3_commands() -> list[dict[str, int | str]]:
    return rbm3_payload_commands()  # type: ignore[return-value]


def media_module(out_dir: Path) -> bytes:
    root = Path(__file__).resolve().parents[1]
    objects = []
    for name in ("media", "media_graphics", "media_driver"):
        obj = out_dir / (name + ".o")
        subprocess.run(["ca65", "-o", str(obj), str(root / "src/apps/readybasic" / (name + ".s"))], check=True)
        objects.append(str(obj))
    binary, labels = out_dir / "media.bin", out_dir / "media.labels"
    subprocess.run(["ld65", "-C", str(root / "cfg/readybasic_media.cfg"), "-o", str(binary), "-Ln", str(labels), *objects], check=True)
    symbols = {line.split()[2].lstrip("."): int(line.split()[1], 16)
               for line in labels.read_text().splitlines() if line.startswith("al ")}
    payload = binary.read_bytes()
    entries = [("MUSTUNE", "mustune", 19), ("MUSPLAY", "musplay", 10),
               ("MUSHALT", "mushalt", 24), ("MUSDROP", "musdrop", 24),
               ("RSCFILE", "rscfile", 19), ("MCFILE", "koaload", 19),
               ("SPRFILE", "sprfile", 19), ("MCLINE", "mcline", 22)]
    # 82 contiguous built-ins end at $1A40; SCRPUT stays at $1FE0.
    return build_module(module_id=6, desc_reu_offset=0x1a40, commands=[
        dict(command_id=(110+i if i < 5 else 111+i), name=name, reu_offset=0x8000,
             submodule_id=24, overlay_id=0, slot_mask=RB_SLOT_PROOF_12,
             payload=payload, payload_size=len(payload),
             entry_offset=symbols[symbol]-0xb000, signature_id=sig)
        for i, (name, symbol, sig) in enumerate(entries)])


# The 128-entry registry retains SCRPUT in its last slot. Production commands
# occupy $1000-$1A3F, media $1A40-$1B3F, and disk demos $1B40-$1F3F.
# Sample1 + sample2 coexist. Sample3 replaces their demo descriptors and carries
# its own COPY/CPYRST entries; built-ins and media are preserved in either case.
SAMPLE_DESC_OFF = 0x1B40
SAMPLE1_LOW = [
    ("CPYRST", "cmd_cpyrst_low", 14),
    ("COPY", "cmd_copy_low", 14),
    ("ECHO1", "cmd_echo1_low", 14),
    ("ADD16", "cmd_add16_low", 2),
    ("HIDDENRAM", "cmd_hiddenram_hidden", 5),
    ("SUMNUMARRAY", "cmd_sumnumarray_low", 6),
    ("RANGENUMARRAY", "cmd_rangenumarray_low", 7),
    ("TEMPSCRATCH", "cmd_tempscratch_low", 11),
    ("FAIL", "cmd_fail_low", 12),
    ("SLOT0", "cmd_slot0_low", 14),
]
SAMPLE2_GROUPS = [
    (0xB000, 34, 0, RB_SLOT_PROOF_2, [("SLOT2", 32)]),
    (0xB100, 35, 0, RB_SLOT_PROOF_12, [("SPAN", 40), ("DM2S", 74)]),
    (0xB200, 36, 1, RB_SLOT_PROOF_2, [("OVL1", 51), ("DOV1", 72)]),
    (0xB300, 36, 2, RB_SLOT_PROOF_2, [("OVL2", 52), ("DOV2", 73)]),
]


def sample_inventory() -> dict[str, list[str]]:
    """Public names without building payloads, used by docs and static checks."""
    return {
        "sample1": [n for n, _, _ in SAMPLE1_LOW] + ["SLOT1", "DM1"],
        "sample2": [n for _, _, _, _, entries in SAMPLE2_GROUPS for n, _ in entries],
        "sample3": ["CPYRST", "COPY"] + [str(c["name"]) for c in rbm3_commands()],
    }


def sample_low_payload(out_dir: Path) -> tuple[bytes, dict[str, int]]:
    root = Path(__file__).resolve().parents[1]
    labels = root / "obj/readybasic.labels"
    symbols = {line.split()[2].lstrip("."): int(line.split()[1], 16)
               for line in labels.read_text().splitlines() if line.startswith("al ")}
    required = ("rb_copy_count", "rb_reu_core_bank", "rb_reu_fetch",
                "rb_reu_c64_lo", "rb_reu_c64_hi", "rb_reu_off_lo", "rb_reu_off_hi",
                "rb_reu_bank", "rb_reu_len_lo", "rb_reu_len_hi")
    (out_dir / "sample_runtime.inc").write_text(
        "; Generated from the matching ReadyBASIC runtime; rebuild packages together.\n" +
        "".join(f"{n} = ${symbols[n]:04X}\n" for n in required))
    obj, binary, label_file = [out_dir / ("sample_low." + ext) for ext in ("o", "bin", "labels")]
    subprocess.run(["ca65", "-I", str(out_dir), "-o", str(obj),
                    str(root / "src/apps/readybasic/sample_low.s")], check=True)
    subprocess.run(["ld65", "-C", str(root / "cfg/readybasic_sample_low.cfg"),
                    "-o", str(binary), "-Ln", str(label_file), str(obj)], check=True)
    entries = {line.split()[2].lstrip("."): int(line.split()[1], 16) - 0xA800
               for line in label_file.read_text().splitlines() if line.startswith("al ")}
    return binary.read_bytes(), entries


def sample_modules(out_dir: Path) -> dict[str, bytes]:
    payload, entries = sample_low_payload(out_dir)
    low = [dict(command_id=128+i, name=name, reu_offset=0xA000,
                submodule_id=32, overlay_id=0, slot_mask=1,
                payload=payload, payload_size=len(payload),
                entry_offset=entries[symbol], signature_id=sig)
           for i, (name, symbol, sig) in enumerate(SAMPLE1_LOW)]

    def group(offset, submodule, overlay, mask, commands, first_id):
        image = b"".join(int_command_payload(value) for _, value in commands)
        return [dict(command_id=first_id+i, name=name, reu_offset=offset,
                     submodule_id=submodule, overlay_id=overlay, slot_mask=mask,
                     payload=image, payload_size=len(image), entry_offset=21*i,
                     signature_id=SIG_SCRCAP)
                for i, (name, _) in enumerate(commands)]

    sample1 = low + group(0xA800, 33, 0, RB_SLOT_PROOF_1,
                          [("SLOT1", 31), ("DM1", 61)], 138)
    sample2 = []
    for offset, submodule, overlay, mask, commands in SAMPLE2_GROUPS:
        sample2 += group(offset, submodule, overlay, mask, commands, 140+len(sample2))
    # Sample3 only needs these two position-independent counter workers.
    # Do not duplicate sample1's scalar/array/allocator workers on disk.
    counter_start = entries["cmd_cpyrst_low"]
    counter_end = entries["cmd_copy_low_end"]
    counter_payload = payload[counter_start:counter_end]
    counters = [dict(command, payload=counter_payload,
                     payload_size=len(counter_payload),
                     entry_offset=int(command["entry_offset"])-counter_start)
                for command in low[:2]]
    return {
        "rbm.sample1.seq": build_module(module_id=7, desc_reu_offset=SAMPLE_DESC_OFF,
                                        commands=sample1),
        "rbm.sample2.seq": build_module(module_id=8, desc_reu_offset=SAMPLE_DESC_OFF+32*len(sample1),
                                        commands=sample2),
        "rbm.sample3.seq": build_module(module_id=9, desc_reu_offset=SAMPLE_DESC_OFF,
                                        commands=counters+rbm3_commands()),
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()
    args.out_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(Path(__file__).with_name('pack_readybasic_images.py'))], check=True)
    modules = {"rb.bad.seq": b"RSID" + bytes(120),
               "rbm.media.seq": media_module(args.out_dir), **sample_modules(args.out_dir)}
    for filename, payload in modules.items():
        (args.out_dir / filename).write_bytes(payload)


if __name__ == "__main__":
    main()
