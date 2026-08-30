#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speeds="${XUZEXTRACT_SPEEDS:-1 16 64}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${XUZEXTRACT_MATRIX_START_LOG:-/tmp/xuzextract_c64u_matrix_${stamp}.log}"
status="${XUZEXTRACT_MATRIX_START_STATUS:-/tmp/xuzextract_c64u_matrix_${stamp}.status}"
frozen_matrix="/tmp/run_xuzextract_c64u_matrix_${stamp}.sh"
frozen_runner="/tmp/run_xuzextract_c64u_matrix_member_${stamp}.sh"

cp "$readyos_root/build_support/run_xuzextract_c64u_matrix.sh" "$frozen_matrix"
cp "$readyos_root/build_support/run_xuzextract_c64u.sh" "$frozen_runner"
chmod +x "$frozen_matrix" "$frozen_runner"
rm -f "$log" "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' XUZEXTRACT_SPEEDS='$speeds' XUZEXTRACT_RUNNER='$frozen_runner' /bin/bash '$frozen_matrix' >'$log' 2>&1; rc=\$?; echo \$rc >'$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned physical xuzextract matrix at: $speeds"
echo "status=$status"
echo "log=$log"
echo "matrix=$frozen_matrix"
echo "runner=$frozen_runner"
