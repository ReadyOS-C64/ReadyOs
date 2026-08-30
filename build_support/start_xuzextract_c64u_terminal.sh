#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZEXTRACT_SPEED_MHZ:-16}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZEXTRACT_START_LOG:-/tmp/xuzextract_c64u_${stamp}.log}"
status="${XUZEXTRACT_START_STATUS:-/tmp/xuzextract_c64u_${stamp}.status}"
frozen="/tmp/run_xuzextract_c64u_${stamp}.sh"

cp "$readyos_root/build_support/run_xuzextract_c64u.sh" "$frozen"
chmod +x "$frozen"
rm -f "$log" "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZEXTRACT_SPEED_MHZ='$speed' /bin/bash '$frozen' '$log' '$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned physical xuzextract at ${speed} MHz"
echo "status=$status"
echo "log=$log"
echo "runner=$frozen"
