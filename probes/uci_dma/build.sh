#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="$root_dir/build/uci_dma_probe"

mkdir -p "$build_dir"

# Prevent a standalone probe from preserving an obsolete UCI state loop.
python3 "$root_dir/build_support/verify_uci_protocol_contract.py"

ca65_flags=()
if [[ "${PROBE_DO_CD:-0}" == "1" ]]; then
  ca65_flags+=("-D" "DO_CD")
fi
image_type="${PROBE_IMAGE_TYPE:-d81}"
case "$image_type" in
  d64)
    ca65_flags+=("-D" "PROBE_D64")
    default_image_name="UCI40.D64"
    ;;
  d81)
    default_image_name="UCI41.D81"
    ;;
  *)
    echo "PROBE_IMAGE_TYPE must be d81 or d64" >&2
    exit 64
    ;;
esac

image_name="${UCI_DMA_IMAGE_NAME:-$default_image_name}"
python3 - "$image_name" "$image_type" "$build_dir/uci_dma_image_name.inc" <<'PY'
import pathlib
import re
import sys

name, image_type, output = sys.argv[1:]
if not re.fullmatch(r"[A-Za-z0-9._-]{1,48}", name):
    raise SystemExit("UCI_DMA_IMAGE_NAME must be a 1-48 character basename")
if not name.lower().endswith("." + image_type):
    raise SystemExit(f"UCI_DMA_IMAGE_NAME must end in .{image_type}")

def asm_bytes(value):
    return ", ".join(f"${byte:02X}" for byte in value.encode("ascii")) + ", 0"

text = "\n".join((
    f"d81_name: .byte {asm_bytes(name)}",
    f"d81_usb1_abs_name: .byte {asm_bytes('/USB1/' + name)}",
    f"d81_usb0_abs_name: .byte {asm_bytes('/USB0/' + name)}",
    "",
))
pathlib.Path(output).write_text(text, encoding="ascii")
PY

if (( ${#ca65_flags[@]} )); then
  ca65 -I "$build_dir" "${ca65_flags[@]}" -o "$build_dir/uci_dma_probe.o" "$root_dir/probes/uci_dma/uci_dma_probe.s"
else
  ca65 -I "$build_dir" -o "$build_dir/uci_dma_probe.o" "$root_dir/probes/uci_dma/uci_dma_probe.s"
fi
ld65 -C "$root_dir/probes/uci_dma/probe.cfg" \
  -o "$build_dir/uci_dma_probe.prg" \
  "$build_dir/uci_dma_probe.o"

python3 - "$build_dir" <<'PY'
import pathlib
import sys

build = pathlib.Path(sys.argv[1])
payloads = [
    ("udma1.prg", 0x11),
    ("udma2.prg", 0x42),
    ("udma3.prg", 0x83),
]
for name, value in payloads:
    (build / name).write_bytes(bytes([0x00, 0x40]) + bytes([value]) * 0x100)
PY

disk="$build_dir/uci_dma_probe.$image_type"
rm -f "$disk"
c1541 -format "uci dma probe,up" "$image_type" "$disk" \
  -write "$build_dir/uci_dma_probe.prg" "probe" \
  -write "$build_dir/udma1.prg" "udma1" \
  -write "$build_dir/udma2.prg" "udma2" \
  -write "$build_dir/udma3.prg" "udma3"

verify_dir="$build_dir/readback"
rm -rf "$verify_dir"
mkdir -p "$verify_dir"

c1541 "$disk" -read "probe" "$verify_dir/probe.prg" >/dev/null
c1541 "$disk" -read "udma1" "$verify_dir/udma1.prg" >/dev/null
c1541 "$disk" -read "udma2" "$verify_dir/udma2.prg" >/dev/null
c1541 "$disk" -read "udma3" "$verify_dir/udma3.prg" >/dev/null

cmp -s "$build_dir/uci_dma_probe.prg" "$verify_dir/probe.prg"
cmp -s "$build_dir/udma1.prg" "$verify_dir/udma1.prg"
cmp -s "$build_dir/udma2.prg" "$verify_dir/udma2.prg"
cmp -s "$build_dir/udma3.prg" "$verify_dir/udma3.prg"

python3 - "$disk" <<'PY'
import pathlib
import sys

disk = pathlib.Path(sys.argv[1])
data = disk.read_bytes()
expected = [
    b"PROBE" + b"\xa0" * 11,
    b"UDMA1" + b"\xa0" * 11,
    b"UDMA2" + b"\xa0" * 11,
    b"UDMA3" + b"\xa0" * 11,
]
missing = [name for name in expected if data.find(name) < 0]
if missing:
    names = " ".join(name[:5].decode("ascii") for name in missing)
    raise SystemExit(f"missing lowercase PETSCII directory names: {names}")
print("verified raw lowercase PETSCII directory names")
PY

image_type_upper="$(printf "%s" "$image_type" | tr '[:lower:]' '[:upper:]')"
echo "verified ${image_type_upper} readback: probe udma1 udma2 udma3"
shasum -a 256 "$disk"

echo "$disk"
