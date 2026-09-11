#!/usr/bin/env bash
set -uo pipefail

host="${C64U_HOST:-10.0.0.79}"
remote_root="${1:-}"
out_dir="${2:-}"
log="${3:-/tmp/xuzdeflate_partial_fetch.log}"
status_file="${4:-/tmp/xuzdeflate_partial_fetch.status}"

case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZDEFLATE-*) ;;
  *) echo "refusing non-owned xuzdeflate root: $remote_root" > "$log"
     echo 64 > "$status_file"
     exit 64 ;;
esac
if [[ -z "$out_dir" ]]; then
  echo "output directory required" > "$log"
  echo 64 > "$status_file"
  exit 64
fi

mkdir -p "$out_dir"
: > "$log"
rc=0
curl --fail --silent --show-error --max-time 60 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/" \
  -o "$out_dir/listing.txt" >> "$log" 2>&1 || rc=1
for name in EMPTY REPEAT RANDOM CROSS; do
  if curl --fail --silent --show-error --max-time 180 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/${name}.RAW" \
    -o "$out_dir/${name}.RAW" >> "$log" 2>&1; then
    stat -f "%N %z bytes" "$out_dir/${name}.RAW" >> "$log" 2>&1
    shasum -a 256 "$out_dir/${name}.RAW" >> "$log" 2>&1
  else
    rm -f "$out_dir/${name}.RAW"
    echo "$name unavailable" >> "$log"
  fi
done
echo "$rc" > "$status_file"
exit "$rc"
