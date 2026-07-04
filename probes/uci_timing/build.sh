#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="$root_dir/build/uci_timing"
profile="$root_dir/cfg/profiles/precog-d81.ini"
base_d81="${BASE_D81:-}"

mkdir -p "$build_dir"

if [[ -z "$base_d81" ]]; then
  base_d81="$(ls -t "$root_dir"/Releases/0.2.5/precog-d81/*.d81 2>/dev/null | head -1 || true)"
fi
if [[ -z "$base_d81" || ! -f "$base_d81" ]]; then
  echo "no base D81 found; set BASE_D81 or build the precog-d81 release first" >&2
  exit 1
fi

python3 "$root_dir/probes/uci_timing/build_asm_probe.py" \
  "$root_dir" \
  "$root_dir/probes/uci_dma/uci_dma_probe.s" \
  "$profile" \
  "$build_dir/uci_timing_probe.s"
python3 "$root_dir/build_support/instrument_launcher_uci_dma_for_timing.py" \
  "$root_dir/src/apps/launcher/launcher_uci_dma.s" \
  "$build_dir/launcher_uci_dma_timed.s"

ca65 -o "$build_dir/uci_timing_probe.o" "$build_dir/uci_timing_probe.s"
ca65 -o "$build_dir/launcher_uci_dma.o" "$build_dir/launcher_uci_dma_timed.s"
ld65 -C "$root_dir/probes/uci_timing/probe.cfg" \
  -o "$build_dir/uci_timing_probe.prg" \
  -m "$build_dir/uci_timing_probe.map" \
  "$build_dir/uci_timing_probe.o" \
  "$build_dir/launcher_uci_dma.o"

python3 "$root_dir/build_support/build_apps_catalog_petscii.py" \
  --input "$profile" \
  --output "$build_dir/apps_cfg_petscii.seq" \
  --variant-asm-output "$build_dir/msg_variant.inc" \
  --reu-config-asm-output "$build_dir/readyos_reu_config.inc"

disk="$build_dir/readyos.d81"
rm -f "$disk"
if [[ "${UCI_TIMING_FRESH_D81:-0}" == "1" ]]; then
  c1541 -format "uci timing,ut" d81 "$disk" \
    -write "$build_dir/apps_cfg_petscii.seq" "apps.cfg,s" \
    -write "$build_dir/uci_timing_probe.prg" "utime"
  python3 - "$root_dir" "$profile" "$build_dir/files_to_write.txt" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
profile = pathlib.Path(sys.argv[2])
out = pathlib.Path(sys.argv[3])
sys.path.insert(0, str(root / "probes" / "uci_timing"))
from build_asm_probe import parse_profile

_, items = parse_profile(profile)
seen = set()
lines = []
for item in items:
    name = str(item["name"]).lower()
    if name in seen:
        continue
    seen.add(name)
    src = root / "bin" / f"{name}.prg"
    if not src.exists():
        raise SystemExit(f"missing {src}")
    lines.append(f"{src}\t{name}")
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
PY
  while IFS=$'\t' read -r src name; do
    [[ -n "$src" ]] || continue
    c1541 "$disk" -write "$src" "$name"
  done < "$build_dir/files_to_write.txt"
else
  cp "$base_d81" "$disk"
  c1541 "$disk" \
    -delete "apps.cfg" \
    -delete "utime" \
    -write "$build_dir/apps_cfg_petscii.seq" "apps.cfg,s" \
    -write "$build_dir/uci_timing_probe.prg" "utime"
fi

verify_dir="$build_dir/readback"
rm -rf "$verify_dir"
mkdir -p "$verify_dir"
(
  cd "$verify_dir"
  c1541 "$disk" -read "utime" >/dev/null
  c1541 "$disk" -read "apps.cfg,s" >/dev/null
)
cmp -s "$build_dir/uci_timing_probe.prg" "$verify_dir/utime"
cmp -s "$build_dir/apps_cfg_petscii.seq" "$verify_dir/apps.cfg"

python3 - "$disk" <<'PY'
import pathlib
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
for raw in (b"UTIME" + b"\xa0" * 11, b"APPS.CFG" + b"\xa0" * 8):
    if data.find(raw) < 0:
        raise SystemExit(f"missing directory entry {raw!r}")
print("verified probe image directory entries")
PY

shasum -a 256 "$disk"
echo "$disk"
