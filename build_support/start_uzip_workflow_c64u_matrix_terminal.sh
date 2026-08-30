#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
speeds="${UZIP_WORKFLOW_SPEEDS:-1 64}"
stamp="$(date +%Y%m%d_%H%M%S)"
log="${UZIP_WORKFLOW_MATRIX_START_LOG:-/tmp/uzip_workflow_c64u_matrix_${stamp}.log}"
status="${UZIP_WORKFLOW_MATRIX_START_STATUS:-/tmp/uzip_workflow_c64u_matrix_${stamp}.status}"
frozen_matrix="/tmp/run_uzip_workflow_c64u_matrix_${stamp}.sh"
frozen_runner="/tmp/run_uzip_workflow_c64u_matrix_member_${stamp}.sh"

cp "$readyos_root/build_support/run_uzip_workflow_c64u_matrix.sh" "$frozen_matrix"
cp "$readyos_root/build_support/run_uzip_workflow_c64u.sh" "$frozen_runner"
chmod +x "$frozen_matrix" "$frozen_runner"
rm -f "$log" "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' UZIP_WORKFLOW_SPEEDS='$speeds' UZIP_WORKFLOW_RUNNER='$frozen_runner' /bin/bash '$frozen_matrix' >'$log' 2>&1; rc=\$?; echo \$rc >'$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned complete uZIP workflow matrix at: $speeds"
echo "status=$status"
echo "log=$log"
echo "matrix=$frozen_matrix"
echo "runner=$frozen_runner"
