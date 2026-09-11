#!/usr/bin/env bash
set -euo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
host="${C64U_HOST:-10.0.0.79}"
out_dir="${1:?usage: start_xuzextract_resume_terminal.sh OUT_DIR RUN_LOG}"
run_log="${2:?usage: start_xuzextract_resume_terminal.sh OUT_DIR RUN_LOG}"
stamp="$(date +%Y%m%d_%H%M%S)"
status="/tmp/xuzextract_resume_${stamp}.status"
frozen="/tmp/resume_xuzextract_downloads_${stamp}.sh"

cp "$readyos_root/build_support/resume_xuzextract_downloads_c64u.sh" "$frozen"
chmod +x "$frozen"
rm -f "$status"
command_text="cd '$readyos_root' && C64U_HOST='$host' /bin/bash '$frozen' '$out_dir' '$run_log' '$status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//"/\\"}"'"' >/dev/null
echo "Started Terminal-owned read-only xuzextract recovery"
echo "status=$status"
echo "runner=$frozen"

