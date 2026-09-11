#!/usr/bin/env bash
# Read-only recovery of the filesystem oracle after a physical control-step
# failure. This must be launched from Terminal, never as a foreground Codex
# network command.
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
run_id="${1:?usage: recover_uzip_workflow_oracle_c64u.sh RUN_ID [OUT_DIR] [LOG] [STATUS]}"
out_dir="${2:-$readyos_root/logs/uzip-workflow/$run_id}"
log="${3:-/tmp/recover_uzip_workflow_${run_id}.log}"
status="${4:-/tmp/recover_uzip_workflow_${run_id}.status}"
remote_root="USB1/READYOS_UZIP_TEST/$run_id"
ftp_base="ftp://anonymous:anonymous%40@${host}"

case "$run_id" in
  UZIPFLOW-[0-9]*-16MHZ-[0-9]*) ;;
  *) echo "refusing non-owned uZIP workflow run id: $run_id" >&2; exit 64 ;;
esac

mkdir -p "$out_dir"
rm -f "$log" "$status"
trap 'rc=$?; echo "$rc" > "$status"; echo "EXIT $rc" >> "$log"' EXIT

fixture_dir="$out_dir/fixtures"
owner_fixture="$out_dir/fixtures/owner.marker"
[[ -f "$fixture_dir/LOOSE.BIN" &&
   -f "$fixture_dir/TREE-ROOT.TXT" &&
   -f "$fixture_dir/TREE-DEEP.BIN" &&
   -f "$owner_fixture" ]] || {
  echo "local owned-run fixtures are missing" >> "$log"
  exit 2
}

run() { echo "+ $*" >> "$log"; "$@" >> "$log" 2>&1; }

{
  date
  echo "Read-only physical C64U uZIP workflow oracle recovery"
  echo "Owned remote root: /$remote_root"
  echo "Storage mutation: none"
  echo "VICE use: forbidden"
} > "$log"

run curl --fail --silent --show-error --max-time 30 \
  "$ftp_base/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.recovery" || exit 2
cmp -s "$owner_fixture" "$out_dir/owner.recovery" || {
  echo "owned marker mismatch" >> "$log"
  exit 2
}

run curl --fail --silent --show-error --max-time 90 \
  "$ftp_base/${remote_root}/OUT/archive.zip" \
  -o "$out_dir/archive.zip" || exit 3
run curl --fail --silent --show-error --max-time 60 --list-only \
  "$ftp_base/${remote_root}/OUT/" -o "$out_dir/output-list.txt" || exit 3
run python3 -m zipfile -t "$out_dir/archive.zip" || exit 4
run python3 - "$fixture_dir" "$out_dir/archive.zip" <<'PY' || exit 4
import sys
import zipfile
from pathlib import Path

fixtures = Path(sys.argv[1])
archive = Path(sys.argv[2])
expected = {
    "LOOSE.BIN": fixtures / "LOOSE.BIN",
    "TREE/ROOT.TXT": fixtures / "TREE-ROOT.TXT",
    "TREE/NEST/DEEP.BIN": fixtures / "TREE-DEEP.BIN",
}
with zipfile.ZipFile(archive) as handle:
    names = {item.filename.upper() for item in handle.infolist()}
    wanted = set(expected) | {"TREE/", "TREE/NEST/"}
    if names != wanted:
        raise SystemExit(f"unexpected members: {sorted(names)!r}")
    for name, fixture in expected.items():
        if handle.read(name) != fixture.read_bytes():
            raise SystemExit(f"archive byte mismatch: {name}")
print("recursive archive structure and bytes passed")
PY

echo "UZIP WORKFLOW RECOVERED ORACLE PASS /$remote_root" | \
  tee "$out_dir/RECOVERED_ORACLE_PASS.txt" >> "$log"
