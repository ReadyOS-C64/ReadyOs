#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZSTORE_SPEED_MHZ:-16}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZSTORE_START_LOG:-/tmp/xuzstore_c64u_${stamp}.log}"
status="${XUZSTORE_START_STATUS:-/tmp/xuzstore_c64u_${stamp}.status}"

rm -f "$log" "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZSTORE_SPEED_MHZ='$speed' /bin/bash build_support/run_xuzstore_c64u.sh '$log' '$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned physical xuzstore at ${speed} MHz"
echo "status=$status"
echo "log=$log"
