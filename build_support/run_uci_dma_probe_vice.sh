#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
if [ -n "${VICE_TASKS_ROOT:-}" ]; then
  VICE_TOOL_ROOT="$(cd "$VICE_TASKS_ROOT" && pwd)"
else
  HARNESS_REPO="${VICE_TASKS_REPO:-$READYOS_ROOT/../agenticdevharness}"
  VICE_TOOL_ROOT="$(cd "$HARNESS_REPO/tools/vice_tasks_dotnet" && pwd)"
fi

PROJECT="$VICE_TOOL_ROOT/src/ViceTasks.Binary/ViceTasks.Binary.csproj"
PLAN="${UCI_DMA_VICE_PLAN:-$SCRIPT_DIR/uci_dma_probe_vice.generated.yaml}"
LOG="${UCI_DMA_VICE_LOG:-/tmp/uci_dma_probe_vice.log}"
STATUS="${UCI_DMA_VICE_STATUS:-/tmp/uci_dma_probe_vice.status}"
RUN_DIR_FILE="${UCI_DMA_VICE_RUN_DIR_FILE:-/tmp/uci_dma_probe_vice.run_dir}"

rm -f "$LOG" "$STATUS" "$RUN_DIR_FILE"

cd "$READYOS_ROOT"
PROBE_IMAGE_TYPE=d81 /bin/bash probes/uci_dma/build.sh >>"$LOG" 2>&1

set +e
dotnet run --project "$PROJECT" -- run-plan --plan "$PLAN" --close-vice --no-tui \
  >>"$LOG" 2>&1
rc=$?
set -e
if [ "$rc" -ne 0 ]; then
  echo "$rc" >"$STATUS"
  exit "$rc"
fi

python3 - "$LOG" "$RUN_DIR_FILE" <<'PY'
import json
import pathlib
import re
import sys

text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8-sig", errors="ignore")
run_dir = ""
for line in reversed(text.splitlines()):
    line = line.strip()
    if not line:
        continue
    try:
        obj = json.loads(line)
    except Exception:
        obj = None
    if isinstance(obj, dict):
        run_dir = str(obj.get("RunDir") or obj.get("run_dir") or "")
        if not run_dir and isinstance(obj.get("artifacts"), dict):
            run_dir = str(obj["artifacts"].get("run_dir") or "")
        if run_dir:
            break
matches = re.findall(r"run_dir[=:] ?`?([^`\"\\s,}]+)", text, re.IGNORECASE)
if not run_dir and matches:
    run_dir = matches[-1]
if not run_dir:
    matches = re.findall(r'"(?:run_dir|RunDir)"\s*:\s*"([^"]+)"', text)
    if matches:
        run_dir = matches[-1]
if not run_dir:
    raise SystemExit("could not find VICE run_dir in log")
pathlib.Path(sys.argv[2]).write_text(run_dir + "\n", encoding="utf-8")
print(run_dir)
PY

run_dir="$(cat "$RUN_DIR_FILE")"
python3 build_support/analyze_uci_dma_probe_run.py \
  "$run_dir" --expect no-uci --version 41 >>"$LOG" 2>&1

echo 0 >"$STATUS"
echo "OK VICE $run_dir" >>"$LOG"
