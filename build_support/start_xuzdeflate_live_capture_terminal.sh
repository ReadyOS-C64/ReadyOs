#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
stamp="$(date +%Y%m%d_%H%M%S)"
out_dir="${XUZDEFLATE_CAPTURE_DIR:-$readyos_root/logs/xuzdeflate/live-$stamp}"
log="${XUZDEFLATE_CAPTURE_LOG:-/tmp/xuzdeflate_live_capture_$stamp.log}"
status_file="${XUZDEFLATE_CAPTURE_STATUS:-/tmp/xuzdeflate_live_capture_$stamp.status}"

rm -f "$log" "$status_file"
command_text="cd '$readyos_root' && C64U_HOST='$host' /bin/bash build_support/capture_xuzdeflate_live_c64u.sh '$out_dir' '$log' '$status_file'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned read-only xuzdeflate live capture"
echo "out=$out_dir"
echo "status=$status_file"
echo "log=$log"
