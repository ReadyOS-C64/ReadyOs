#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
remote_root="${1:-}"
stamp="$(date +%Y%m%d_%H%M%S)"
out_dir="${XUZDEFLATE_PARTIAL_DIR:-$readyos_root/logs/xuzdeflate/partial-$stamp}"
log="${XUZDEFLATE_PARTIAL_LOG:-/tmp/xuzdeflate_partial_fetch_$stamp.log}"
status_file="${XUZDEFLATE_PARTIAL_STATUS:-/tmp/xuzdeflate_partial_fetch_$stamp.status}"

rm -f "$log" "$status_file"
command_text="cd '$readyos_root' && C64U_HOST='$host' /bin/bash build_support/fetch_xuzdeflate_partial_c64u.sh '$remote_root' '$out_dir' '$log' '$status_file'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned owned-root xuzdeflate partial fetch"
echo "out=$out_dir"
echo "status=$status_file"
echo "log=$log"
