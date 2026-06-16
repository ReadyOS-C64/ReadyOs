#!/bin/bash
cd /Users/karlprosserpp/dev/c64projects/readyosprecog || exit 1
export DISABLE_AUTO_UPDATE=true
export READYBASIC_HOTKEY_HOST_KEYS=1
/bin/bash build_support/run_readybasic_hotkey_probe.sh > agentworking/readybasic_hotkeys_fix/terminal_probe.log 2>&1
status=$?
printf 'EXIT:%s\n' "$status" > agentworking/readybasic_hotkeys_fix/terminal_probe.status
exit "$status"
