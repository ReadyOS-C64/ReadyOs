#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
remote_root="${1:-}"
out_dir="${2:-}"
log="${3:-/tmp/xuzinflate_partial_fetch.log}"
status_file="${4:-/tmp/xuzinflate_partial_fetch.status}"

case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZINFLATE-*) ;;
  *) echo "refusing non-owned xuzinflate root: $remote_root" > "$log";
     echo 64 > "$status_file"; exit 64 ;;
esac
if [[ -z "$out_dir" ]]; then
  echo "output directory required" > "$log"
  echo 64 > "$status_file"
  exit 64
fi

mkdir -p "$out_dir"
rc=0
: > "$log"
for name in EMPTY STORED FIXED DYNAMIC; do
  if curl --fail --silent --show-error --max-time 180 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/${name}.BIN" \
    -o "$out_dir/${name}.BIN" >> "$log" 2>&1; then
    shasum -a 256 "$out_dir/${name}.BIN" >> "$log" 2>&1
  else
    echo "$name unavailable" >> "$log"
    rc=1
  fi
done
echo "$rc" > "$status_file"
exit "$rc"
