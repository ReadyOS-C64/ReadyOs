#!/usr/bin/env bash
set -euo pipefail

READYOS_ROOT="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
HARNESS="${VICE_TASKS_ROOT:-$READYOS_ROOT/../agenticdevharness/tools/vice_tasks_dotnet}"
HOST="${C64U_HOST:-10.0.0.79}"
SOURCE="${SETUP_C64U_D81:-$READYOS_ROOT/Releases/0.5/precog-ultimate/readyos-v0.5-ultimate.d81}"
OUT="${SETUP_C64U_FAILURE_OUT_DIR:-$READYOS_ROOT/logs/setup_c64u_prereq_failures}"
RUN_TAG="$(date +%Y%m%d-%H%M%S)-$$"
CONFIG_URL="http://${HOST}/v1/configs"
ORIGINAL="$OUT/original-${RUN_TAG}.json"
RESTORE="$OUT/restore-${RUN_TAG}.json"

case "$SOURCE" in
  "$READYOS_ROOT"/*) ;;
  *) echo "SETUP_C64U_D81 must be inside the ReadyOS workspace" >&2; exit 64 ;;
esac
test -f "$SOURCE"
mkdir -p "$OUT" "$READYOS_ROOT/build/setup_ultimate"

curl --fail --silent --show-error --max-time 30 \
  "${CONFIG_URL}/C64%20and%20Cartridge%20Settings/*" -o "$ORIGINAL"
python3 - "$ORIGINAL" "$RESTORE" <<'PY'
import json, pathlib, sys
source = json.loads(pathlib.Path(sys.argv[1]).read_text())
settings = source["C64 and Cartridge Settings"]
payload = {"C64 and Cartridge Settings": {
    "RAM Expansion Unit": settings["RAM Expansion Unit"]["current"],
    "REU Size": settings["REU Size"]["current"],
    "Command Interface": settings["Command Interface"]["current"],
}}
pathlib.Path(sys.argv[2]).write_text(json.dumps(payload))
PY

post_settings() {
  local reu="$1"
  local command_interface="$2"
  python3 - "$reu" "$command_interface" <<'PY' | \
    curl --fail --silent --show-error --max-time 30 -X POST \
      -H 'Content-Type: application/json' --data-binary @- "$CONFIG_URL" >/dev/null
import json, sys
print(json.dumps({"C64 and Cartridge Settings": {
    "RAM Expansion Unit": sys.argv[1],
    "REU Size": "16 MB",
    "Command Interface": sys.argv[2],
}}))
PY
}

restore_settings() {
  curl --fail --silent --show-error --max-time 30 -X POST \
    -H 'Content-Type: application/json' --data-binary "@$RESTORE" \
    "$CONFIG_URL" >/dev/null || true
  curl --fail --silent --show-error --max-time 30 -X PUT \
    "http://${HOST}/v1/machine:reset" >/dev/null || true
}
trap restore_settings EXIT HUP INT TERM

fixture="$READYOS_ROOT/build/setup_ultimate/prereq-${RUN_TAG}.d81"
python3 "$READYOS_ROOT/build_support/prepare_setup_fixture.py" \
  --source "$SOURCE" --output "$fixture" --saved-path ""

run_case() {
  local label="$1"
  local expected="$2"
  local remote_dir="USB1/READYOS_SETUP_TEST/${RUN_TAG}/PREREQ-${label}"
  local remote_image="${remote_dir}/PREREQ-${label}.D81"
  local plan="$OUT/${RUN_TAG}-${label}.yaml"
  local log="$OUT/${RUN_TAG}-${label}.log"
  local attempt

  sed \
    -e "s#host: 10.0.0.79#host: ${HOST}#g" \
    -e "s#remote_root: USB1/READYOS_SETUP_TEST/RUN#remote_root: ${remote_dir}#g" \
    -e "s#prg: bin/setup.prg#prg: ${READYOS_ROOT}/bin/setup.prg#g" \
    -e "s#build/setup_ultimate/fixture.d81#${fixture}#g" \
    -e "s#USB1/READYOS_SETUP_TEST/RUN/fixture.d81#${remote_image}#g" \
    -e "s#SETUP_EXPECTED_TEXT#${expected}#g" \
    "$READYOS_ROOT/build_support/setup_ultimate_prereq_failure.generated.yaml" > "$plan"

  for attempt in 1 2; do
    if (cd "$HARNESS" && ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
      --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
      -- run-ultimate-plan --plan "$plan" --no-tui) >"$log" 2>&1; then
      return 0
    fi
  done
  cat "$log" >&2
  return 1
}

post_settings "Enabled" "Disabled"
run_case "UCI-OFF" "ENABLE COMMAND INTERFACE/UCI"

post_settings "Disabled" "Enabled"
run_case "REU-OFF" "ENABLE REU"

restore_settings
trap - EXIT HUP INT TERM
echo "SETUP C64U prerequisite failure UI passed: UCI disabled, REU disabled" | \
  tee "$OUT/result-${RUN_TAG}.txt"
