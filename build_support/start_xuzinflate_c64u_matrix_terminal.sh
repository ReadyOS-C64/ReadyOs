#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speeds="${XUZINFLATE_SPEEDS:-1 16 64}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZINFLATE_MATRIX_START_LOG:-/tmp/xuzinflate_c64u_matrix_${stamp}.log}"
status="${XUZINFLATE_MATRIX_START_STATUS:-/tmp/xuzinflate_c64u_matrix_${stamp}.status}"

rm -f "$log" "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZINFLATE_SPEEDS='$speeds' /bin/bash build_support/run_xuzinflate_c64u_matrix.sh >'$log' 2>&1; rc=\$?; echo \$rc >'$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned physical xuzinflate matrix at: $speeds"
echo "status=$status"
echo "log=$log"
