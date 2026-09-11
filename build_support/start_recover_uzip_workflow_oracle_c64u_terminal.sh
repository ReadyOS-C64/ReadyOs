#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
run_id="${1:?usage: start_recover_uzip_workflow_oracle_c64u_terminal.sh RUN_ID}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="/tmp/recover_uzip_workflow_${stamp}.log"
status="/tmp/recover_uzip_workflow_${stamp}.status"
runner="/tmp/recover_uzip_workflow_${stamp}.sh"

cp "$readyos_root/build_support/recover_uzip_workflow_oracle_c64u.sh" "$runner"
chmod +x "$runner"
command_text="cd '$readyos_root' && C64U_HOST='$host' /bin/bash '$runner' '$run_id' '$readyos_root/logs/uzip-workflow/$run_id' '$log' '$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//\"/\\\"}"'"' >/dev/null
echo "Started Terminal-owned read-only uZIP oracle recovery"
echo "status=$status"
echo "log=$log"
