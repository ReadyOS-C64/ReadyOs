#!/usr/bin/env bash
set -euo pipefail
command_text="curl --fail --silent --show-error --max-time 10 'http://10.0.0.79/v1/machine:readmem?address=07C0&length=40' -o '/tmp/launcher_probe_screenbytes_20260822_r9.bin'; rc=\$?; xxd -g 1 '/tmp/launcher_probe_screenbytes_20260822_r9.bin' > '/tmp/launcher_probe_screenbytes_20260822_r9.txt'; echo \$rc > '/tmp/launcher_probe_screenbytes_20260822_r9.status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//\"/\\\"}"'"' >/dev/null
