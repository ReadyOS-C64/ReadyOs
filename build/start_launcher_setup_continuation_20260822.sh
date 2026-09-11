#!/usr/bin/env bash
set -euo pipefail
command_text="cd '/Users/karlprosserpp/dev/c64projects/agenticdevharness/tools/vice_tasks_dotnet' && ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run --project src/ViceTasks.Binary/ViceTasks.Binary.csproj -- run-ultimate-plan --plan '/Users/karlprosserpp/dev/c64projects/readyosprecog/build/launcher_setup_continuation_20260822.yaml' --no-tui >'/tmp/launcher_setup_continuation_20260822.log' 2>&1; rc=\$?; echo \$rc >'/tmp/launcher_setup_continuation_20260822.status'"
osascript -e 'tell application "Terminal" to do script "'"${command_text//\"/\\\"}"'"' >/dev/null
