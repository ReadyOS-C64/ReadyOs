#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZINFLATE_SPEED_MHZ:-16}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZINFLATE_START_LOG:-/tmp/xuzinflate_c64u_${stamp}.log}"
status="${XUZINFLATE_START_STATUS:-/tmp/xuzinflate_c64u_${stamp}.status}"
runner_snapshot="/tmp/run_xuzinflate_c64u_${stamp}.sh"

rm -f "$log" "$status" "$runner_snapshot"
cp "$readyos_root/build_support/run_xuzinflate_c64u.sh" "$runner_snapshot"
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZINFLATE_SPEED_MHZ='$speed' /bin/bash '$runner_snapshot' '$log' '$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned physical xuzinflate at ${speed} MHz"
echo "status=$status"
echo "log=$log"
echo "runner=$runner_snapshot"
