#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZCREATEPLAN_SPEED_MHZ:-16}"
quiet_s="${XUZCREATEPLAN_QUIET_S:-}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZCREATEPLAN_START_LOG:-/tmp/xuzcreateplan_c64u_${stamp}.log}"
status="${XUZCREATEPLAN_START_STATUS:-/tmp/xuzcreateplan_c64u_${stamp}.status}"
frozen="/tmp/run_xuzcreateplan_c64u_${stamp}.sh"

cp "$readyos_root/build_support/run_xuzcreateplan_c64u.sh" "$frozen"
chmod +x "$frozen"
rm -f "$log" "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZCREATEPLAN_SPEED_MHZ='$speed' XUZCREATEPLAN_QUIET_S='$quiet_s' /bin/bash '$frozen' '$log' '$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned physical xuzcreateplan at ${speed} MHz"
echo "status=$status"
echo "log=$log"
echo "runner=$frozen"
