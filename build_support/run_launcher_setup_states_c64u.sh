#!/usr/bin/env bash
set -euo pipefail

READYOS_ROOT="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
HARNESS="${VICE_TASKS_ROOT:-$READYOS_ROOT/../agenticdevharness/tools/vice_tasks_dotnet}"
HOST="${C64U_HOST:-10.0.0.79}"
STREAM_PORT="${LAUNCHER_SETUP_STREAM_PORT:-12000}"
SOURCE="${LAUNCHER_SETUP_D81:-$READYOS_ROOT/Releases/0.5/precog-ultimate/readyos-v0.5-ultimate.d81}"
OUT="${LAUNCHER_SETUP_OUT_DIR:-$READYOS_ROOT/logs/launcher_setup_states_c64u}"
RUN_TAG="$(date +%Y%m%d-%H%M%S)-$$"
REMOTE_DIR="USB1/READYOS_SETUP_TEST/LAUNCHER-${RUN_TAG}"
WORK="$READYOS_ROOT/build/setup_ultimate/launcher-${RUN_TAG}"
INVALID="$WORK/launcher-invalid.d81"
VALID="$WORK/launcher-valid.d81"
PLAN="$OUT/launcher-setup-states.yaml"

case "$SOURCE" in
  "$READYOS_ROOT"/*) ;;
  *) echo "LAUNCHER_SETUP_D81 must be inside the ReadyOS workspace" >&2; exit 64 ;;
esac
test -f "$SOURCE"
mkdir -p "$OUT" "$WORK"

python3 "$READYOS_ROOT/build_support/prepare_setup_fixture.py" \
  --source "$SOURCE" --output "$INVALID" \
  --saved-path "/${REMOTE_DIR}/MISSING.D81"
python3 "$READYOS_ROOT/build_support/prepare_setup_fixture.py" \
  --source "$SOURCE" --output "$VALID" \
  --saved-path "/${REMOTE_DIR}/VALID.D81"

sed \
  -e "s#host: 10.0.0.79#host: ${HOST}#g" \
  -e "s#stream_port: 12000#stream_port: ${STREAM_PORT}#g" \
  -e "s#USB1/READYOS_SETUP_TEST/LAUNCHER-RUN#${REMOTE_DIR}#g" \
  -e "s#build/setup_ultimate/launcher-invalid.d81#${INVALID}#g" \
  -e "s#build/setup_ultimate/launcher-valid.d81#${VALID}#g" \
  "$READYOS_ROOT/build_support/launcher_setup_states_ultimate.generated.yaml" > "$PLAN"

(cd "$HARNESS" && ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$PLAN" --no-tui) | tee "$OUT/run.log"

echo "Launcher missing/valid SETUP path states passed on physical C64 Ultimate." \
  | tee "$OUT/result.txt"
