#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZDEFLATE_SPEED_MHZ:-16}"
quiet_s="${XUZDEFLATE_QUIET_S:-}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZDEFLATE_START_LOG:-/tmp/xuzdeflate_c64u_${stamp}.log}"
status_file="${XUZDEFLATE_START_STATUS:-/tmp/xuzdeflate_c64u_${stamp}.status}"
runner_snapshot="/tmp/run_xuzdeflate_c64u_${stamp}.sh"

rm -f "$log" "$status_file" "$runner_snapshot"
cp "$readyos_root/build_support/run_xuzdeflate_c64u.sh" "$runner_snapshot"
case "$quiet_s" in *[!0-9]*) echo "quiet seconds must be an integer" >&2; exit 64 ;; esac
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZDEFLATE_SPEED_MHZ='$speed' XUZDEFLATE_QUIET_S='$quiet_s' /bin/bash '$runner_snapshot' '$log' '$status_file'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned physical xuzdeflate at ${speed} MHz"
echo "status=$status_file"
echo "log=$log"
echo "runner=$runner_snapshot"
