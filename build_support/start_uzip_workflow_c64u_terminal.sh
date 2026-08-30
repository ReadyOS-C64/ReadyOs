#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speed="${UZIP_WORKFLOW_SPEED_MHZ:-16}"
case "$speed" in
  1|16|64) ;;
  *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;;
esac
stamp="$(date +%Y%m%d_%H%M%S)"
log="${UZIP_WORKFLOW_START_LOG:-/tmp/uzip_workflow_c64u_${speed}mhz_${stamp}.log}"
status="${UZIP_WORKFLOW_START_STATUS:-/tmp/uzip_workflow_c64u_${speed}mhz_${stamp}.status}"
frozen="/tmp/run_uzip_workflow_c64u_${stamp}.sh"

cp "$readyos_root/build_support/run_uzip_workflow_c64u.sh" "$frozen"
chmod +x "$frozen"
rm -f "$log" "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' UZIP_WORKFLOW_SPEED_MHZ='$speed' /bin/bash '$frozen' '$log' '$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//\"/\\\"}"'"' >/dev/null
echo "Started Terminal-owned complete uZIP workflow at ${speed} MHz"
echo "status=$status"
echo "log=$log"
echo "runner=$frozen"
