#!/usr/bin/env bash
set -euo pipefail

READYOS_ROOT="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
LOG="${SETUP_C64U_FAILURE_START_LOG:-/tmp/setup_c64u_prereq_terminal.log}"
STATUS="${SETUP_C64U_FAILURE_START_STATUS:-/tmp/setup_c64u_prereq_terminal.status}"
HOST="${C64U_HOST:-10.0.0.79}"
rm -f "$LOG" "$STATUS"

command_text="cd '$READYOS_ROOT' && C64U_HOST='$HOST' /bin/bash build_support/run_setup_c64u_prereq_failures.sh >'$LOG' 2>&1; rc=\$?; echo \$rc >'$STATUS'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//\"/\\\"}"'"' >/dev/null
echo "Started Terminal-owned SETUP prerequisite failures; status: $STATUS; log: $LOG"
