#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speeds="${XUZDEFLATE_SPEEDS:-1 16 64}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZDEFLATE_MATRIX_START_LOG:-/tmp/xuzdeflate_matrix_${stamp}.log}"
status_file="${XUZDEFLATE_MATRIX_START_STATUS:-/tmp/xuzdeflate_matrix_${stamp}.status}"

rm -f "$log" "$status_file"
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZDEFLATE_SPEEDS='$speeds' /bin/bash build_support/run_xuzdeflate_c64u_matrix.sh >'$log' 2>&1; rc=\$?; echo \$rc >'$status_file'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned xuzdeflate matrix at: $speeds"
echo "status=$status_file"
echo "log=$log"
