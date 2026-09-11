#!/usr/bin/env bash
set -euo pipefail

# Read-only recovery for a physical extraction run whose C64 result passed but
# whose host oracle stopped before FTP downloads.  This never mutates Ultimate
# storage and still requires the exact owned-root marker before reading output.
readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
out_dir="${1:?usage: resume_xuzextract_downloads_c64u.sh OUT_DIR RUN_LOG [STATUS]}"
run_log="${2:?usage: resume_xuzextract_downloads_c64u.sh OUT_DIR RUN_LOG [STATUS]}"
status_file="${3:-$out_dir/resume.status}"
run_id="$(basename "$out_dir")"

case "$out_dir" in
  "$readyos_root"/logs/xuzextract/XUZEXTRACT-*) ;;
  *) echo "refusing non-owned local run directory: $out_dir" >&2; exit 64 ;;
esac
case "$run_id" in
  XUZEXTRACT-*) ;;
  *) echo "refusing non-owned run id: $run_id" >&2; exit 64 ;;
esac

remote_root="USB1/READYOS_UZIP_TEST/$run_id"
fixtures="$out_dir/fixtures"
downloads="$out_dir/downloads"
resume_log="$out_dir/resume-downloads.log"
mkdir -p "$downloads"
rm -f "$status_file"
finish() {
  rc=$?
  trap - EXIT
  echo "$rc" > "$status_file"
  exit "$rc"
}
trap finish EXIT

curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.resume" >> "$resume_log" 2>&1
cmp -s "$fixtures/owner.marker" "$out_dir/owner.resume"

for item in \
  "OUT/:out.list" \
  "OUT/NEST/:nest.list" \
  "OUT/NEST/DEEP/:deep.list" \
  "OUT/EXISTING/:existing.list" \
  "OUT/BAD/:bad.list"; do
  path="${item%%:*}"
  file="${item#*:}"
  curl --fail --silent --show-error --max-time 30 --list-only \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/${path}" \
    -o "$downloads/$file" >> "$resume_log" 2>&1
done
curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUT/NEST/STORE.BIN" \
  -o "$downloads/STORE.BIN" >> "$resume_log" 2>&1
curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUT/NEST/DEEP/DEFLATE.BIN" \
  -o "$downloads/DEFLATE.BIN" >> "$resume_log" 2>&1
curl --fail --silent --show-error --max-time 60 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUT/EXISTING/KEEP.BIN" \
  -o "$downloads/KEEP.BIN" >> "$resume_log" 2>&1

cd "$readyos_root"
python3 build_support/analyze_xuzextract_run.py \
  "$run_log" "$fixtures" "$downloads" \
  --json-output "$out_dir/result.json" >> "$resume_log" 2>&1
printf 'XUZEXTRACT PHYSICAL PASS recovered /%s\n' "$remote_root" \
  > "$out_dir/PASS.txt"
