#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

PLAN="tools/vice_tasks/plans/readyshell_crash_probe_headless.yaml"
COMPILED=""
RUN_ROOT=""
COMPARE_ROOT=""
RUN_PROBE=1
WITH_STABILITY=1
PARSE_TRACE_DEBUG=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --plan)
      PLAN="$2"
      shift 2
      ;;
    --compiled)
      COMPILED="$2"
      shift 2
      ;;
    --run-root)
      RUN_ROOT="$2"
      RUN_PROBE=0
      shift 2
      ;;
    --compare-root)
      COMPARE_ROOT="$2"
      shift 2
      ;;
    --no-probe)
      RUN_PROBE=0
      shift
      ;;
    --no-stability)
      WITH_STABILITY=0
      shift
      ;;
    --parse-trace-debug)
      PARSE_TRACE_DEBUG="$2"
      if [[ "$PARSE_TRACE_DEBUG" != "0" && "$PARSE_TRACE_DEBUG" != "1" ]]; then
        echo "error: --parse-trace-debug must be 0 or 1" >&2
        exit 2
      fi
      shift 2
      ;;
    *)
      echo "unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

cd "$REPO_ROOT"

if [[ -n "$PARSE_TRACE_DEBUG" ]]; then
  export READYSHELL_PARSE_TRACE_DEBUG="$PARSE_TRACE_DEBUG"
  echo "[profile] READYSHELL_PARSE_TRACE_DEBUG=${READYSHELL_PARSE_TRACE_DEBUG}"
fi

probe_rc=0
if [[ "$RUN_PROBE" -eq 1 ]]; then
  set +e
  if [[ -n "$COMPILED" ]]; then
    dotnet run --project tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj -- \
      run-compiled --compiled "$COMPILED"
    probe_rc=$?
  else
    dotnet run --project tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj -- \
      run-plan --plan "$PLAN"
    probe_rc=$?
  fi
  set -e
  if [[ "$probe_rc" -ne 0 ]]; then
    echo "[probe] non-zero exit (${probe_rc}); continuing with latest artifacts"
  fi
fi

if [[ -z "$RUN_ROOT" ]]; then
  RUN_ROOT="$(ls -dt logs/vice_auto_* 2>/dev/null | head -n 1 || true)"
fi

if [[ -z "$RUN_ROOT" || ! -d "$RUN_ROOT" ]]; then
  echo "error: could not resolve run root" >&2
  exit 3
fi

echo "[artifact] run_root=${RUN_ROOT}"
summary_cmd=(
  python3
  skills/readyos-vice-dotnet-artifact-analyst/scripts/summarize_dotnet_vice_artifacts.py
  --run-root
  "$RUN_ROOT"
)
if [[ -n "$COMPARE_ROOT" ]]; then
  summary_cmd+=(--compare-root "$COMPARE_ROOT")
fi
"${summary_cmd[@]}"

if [[ "$WITH_STABILITY" -eq 1 ]]; then
  echo "[stability] running companion analyzer"
  skills/readyos-stability-analyst/scripts/run_analysis.sh --mode artifact --run-root "$RUN_ROOT"
fi

exit 0
