#!/usr/bin/env python3
"""Build ReadyBASIC disk-loadable SEQ command module packages."""

from __future__ import annotations

import argparse
import subprocess
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


def stateful_int_entry(value: int, state_addr: int) -> bytes:
    """Return one entrypoint that increments overlay-local state before returning."""
    return bytes(
        [
            0xEE,
            state_addr & 0xFF,
            state_addr >> 8,  # inc state
            0xA9,
            0x00,
            0x8D,
            0x00,
            0xC3,  # RF_STATUS = 0
            0xA9,
            0x01,
            0x8D,
            0x02,
            0xC3,  # RF_TAG = RB_VAL_INT
            0xA9,
            value & 0xFF,
            0x18,
            0x6D,
            state_addr & 0xFF,
            state_addr >> 8,  # value + state
            0x8D,
            0x03,
            0xC3,
            0xA9,
            value >> 8,
            0x69,
            0x00,
            0x8D,
            0x04,
            0xC3,
            0x60,
        ]
    )


def stateful_overlay_payload(value_a: int, value_b: int, runtime_base: int) -> tuple[bytes, int]:
    """Return two entrypoints plus one shared byte of overlay-local state."""
    entry_len = 30
    state_addr = runtime_base + (entry_len * 2)
    payload_a = stateful_int_entry(value_a, state_addr)
    payload_b = stateful_int_entry(value_b, state_addr)
    if len(payload_a) != entry_len or len(payload_b) != entry_len:
        raise ValueError("stateful payload entry length changed")
    return payload_a + payload_b + b"\x00", len(payload_a)


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
    command_id = 33
    reu_offset = 0x3800
    for submodule_id, slot_mask in (
        (6, RB_SLOT_PROOF_2),
        (7, RB_SLOT_PROOF_2),
        (8, RB_SLOT_PROOF_12),
    ):
        submodule_name = {6: "ZS", 7: "ZT", 8: "ZU"}[submodule_id]
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
    # BORDER=$1BC0, USPEED=$1BE0; keep media beyond both built-in descriptors.
    return build_module(module_id=6, desc_reu_offset=0x1c00, commands=[
        dict(command_id=(110+i if i < 5 else 111+i), name=name, reu_offset=0x8000,
             submodule_id=24, overlay_id=0, slot_mask=RB_SLOT_PROOF_12,
             payload=payload, payload_size=len(payload),
             entry_offset=symbols[symbol]-0xb000, signature_id=sig)
        for i, (name, symbol, sig) in enumerate(entries)])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", required=True, type=Path)
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    modules = {
        "rb.bad.seq": b"RSID" + bytes(120),
        "rbm.media.seq": media_module(args.out_dir),
        "rbm.sample1.seq": build_module(
            module_id=3,
            desc_reu_offset=0x1500,
            commands=[
                {
                    "command_id": 29,
                    "name": "ZDM1",
                    "return_value": 61,
                    "reu_offset": 0x3000,
                    "submodule_id": 1,
                    "overlay_id": 0,
                    "slot_mask": RB_SLOT_PROOF_1,
                }
            ],
        ),
        "rbm.sample2.seq": build_module(
            module_id=4,
            desc_reu_offset=0x1600,
            commands=[
                {
                    "command_id": 30,
                    "name": "ZDM2S",
                    "return_value": 74,
                    "reu_offset": 0x3200,
                    "submodule_id": 2,
                    "overlay_id": 0,
                    "slot_mask": RB_SLOT_PROOF_12,
                },
                {
                    "command_id": 31,
                    "name": "ZDOV1",
                    "return_value": 72,
                    "reu_offset": 0x3300,
                    "submodule_id": 5,
                    "overlay_id": 1,
                    "slot_mask": RB_SLOT_PROOF_2,
                },
                {
                    "command_id": 32,
                    "name": "ZDOV2",
                    "return_value": 73,
                    "reu_offset": 0x3400,
                    "submodule_id": 5,
                    "overlay_id": 2,
                    "slot_mask": RB_SLOT_PROOF_2,
                },
            ],
        ),
        "rbm.sample3.seq": build_module(
            module_id=5,
            desc_reu_offset=0x1700,
            commands=rbm3_commands(),
        ),
    }
    for filename, payload in modules.items():
        (args.out_dir / filename).write_bytes(payload)


if __name__ == "__main__":
    main()
