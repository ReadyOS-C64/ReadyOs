#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
archive_mode="${XUZDEFLATE_ARCHIVE_MODE:-raw}"
speed="${XUZDEFLATE_SPEED_MHZ:-16}"
stream_port="${XUZDEFLATE_STREAM_PORT:-12300}"
log="${1:-/tmp/xuzdeflate_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/xuzdeflate_c64u_${speed}mhz.status}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
quiet_s="${XUZDEFLATE_QUIET_S:-}"
case "$archive_mode" in
  raw)
    probe_upper="XUZDEFLATE"
    probe_lower="xuzdeflate"
    output_ext="RAW"
    diagnostic_setting="UZIP_DEFLATE_DIAGNOSTIC=1"
    pass_screen="XUZDEFLATE FINISHED PASS"
    ;;
  zip8)
    probe_upper="XUZZIP8"
    probe_lower="xuzzip8"
    output_ext="ZIP"
    diagnostic_setting="UZIP_ZIP8_DIAGNOSTIC=1"
    pass_screen="XUZZIP8 FINISHED PASS"
    ;;
  zipmulti)
    probe_upper="XUZMULTI"
    probe_lower="xuzmulti"
    output_ext="ZIP"
    diagnostic_setting="UZIP_ZIPMULTI_DIAGNOSTIC=1"
    pass_screen="XUZMULTI FINISHED PASS"
    ;;
  *) echo "archive mode must be raw, zip8, or zipmulti" >&2; exit 64 ;;
esac
run_id="${probe_upper}-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${XUZDEFLATE_OUT_DIR:-$readyos_root/logs/$probe_lower/$run_id}"
fixtures="$out_dir/fixtures"
outputs="$out_dir/outputs"
boot_plan="$out_dir/${probe_lower}-boot-plan.yaml"
codec_plan="$out_dir/${probe_lower}-codec-plan.yaml"

case "$speed" in 1|16|64) ;; *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;; esac
if [[ -z "$quiet_s" ]]; then
  case "$speed" in 1) quiet_s=1800 ;; 16|64) quiet_s=600 ;; esac
fi
case "$quiet_s" in *[!0-9]*|'') echo "quiet seconds must be an integer" >&2; exit 64 ;; esac
case "$remote_root" in
  "USB1/READYOS_UZIP_TEST/${probe_upper}-"*) ;;
  *) echo "refusing non-owned $probe_lower root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$fixtures" "$outputs" "$readyos_root/obj/xuzdeflate"
rm -f "$log" "$status_file"
speed_saved=0
original_turbo_encoded=""
original_speed_encoded=""
finish() {
  rc=$?
  trap - EXIT
  if [[ "$speed_saved" == "1" ]]; then
    restore_rc=0
    curl --fail --silent --show-error --max-time 15 -X PUT \
      "http://${host}/v1/machine:reboot" >> "$log" 2>&1 || restore_rc=1
    sleep 8
    curl --fail --silent --show-error --max-time 15 -X PUT \
      "http://${host}/v1/configs/U64%20Specific%20Settings/Turbo%20Control?value=Manual" \
      >> "$log" 2>&1 || restore_rc=1
    curl --fail --silent --show-error --max-time 15 -X PUT \
      "http://${host}/v1/configs/U64%20Specific%20Settings/CPU%20Speed?value=${original_speed_encoded}" \
      >> "$log" 2>&1 || restore_rc=1
    curl --fail --silent --show-error --max-time 15 -X PUT \
      "http://${host}/v1/configs/U64%20Specific%20Settings/Turbo%20Control?value=${original_turbo_encoded}" \
      >> "$log" 2>&1 || restore_rc=1
    if curl --fail --silent --show-error --max-time 15 \
      "http://${host}/v1/configs/U64%20Specific%20Settings" \
      -o "$out_dir/speed-after.json" >> "$log" 2>&1; then
      python3 - "$speed_before" "$out_dir/speed-after.json" >> "$log" 2>&1 <<'PY' || restore_rc=1
import json
import sys

before = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
after = json.load(open(sys.argv[2], encoding="utf-8"))["U64 Specific Settings"]
for key in ("Turbo Control", "CPU Speed"):
    if str(before[key]) != str(after[key]):
        raise SystemExit(f"CPU restore mismatch for {key}: {after[key]!r} != {before[key]!r}")
PY
    else
      restore_rc=1
    fi
    if [[ "$restore_rc" != "0" ]]; then
      echo "CPU speed restoration failed" >> "$log"
      [[ "$rc" == "0" ]] && rc=6
    else
      echo "CPU speed settings restored" >> "$log"
    fi
  fi
  echo "$rc" > "$status_file"
  echo "EXIT $rc" >> "$log"
  exit "$rc"
}
trap finish EXIT
run() { echo "+ $*" >> "$log"; "$@" >> "$log" 2>&1; }
wait_for_url() {
  label="$1"; url="$2"; deadline=$((SECONDS + connect_wait_s))
  while [ "$SECONDS" -lt "$deadline" ]; do
    if curl --fail --silent --show-error --max-time 5 "$url" >/dev/null 2>>"$log"; then
      echo "$label reachable" >> "$log"; return 0
    fi
    sleep 3
  done
  echo "$label unavailable" >> "$log"; return 1
}
require_no_rest_errors() {
  python3 - "$1" >> "$log" 2>&1 <<'PY'
import json
import sys

reply = json.load(open(sys.argv[1], encoding="utf-8"))
errors = reply.get("errors", [])
if errors:
    raise SystemExit(f"Ultimate REST errors: {errors!r}")
PY
}

{
  date
  echo "Physical C64 Ultimate $probe_lower full-ReadyOS diagnostic"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
  echo "Emulator use: forbidden"
} > "$log"

cd "$readyos_root" || exit 1
run python3 build_support/build_xuzdeflate_fixture.py "$fixtures" \
  --run-id "$run_id" || exit 1
run python3 build_support/build_xuzdeflate_config.py \
  --run-id "$run_id" --volume USB1 --fixture-dir "$fixtures" \
  --output obj/xuzdeflate/xuzdeflate_config.h || exit 1
run python3 build_support/verify_uci_protocol_contract.py || exit 1
run env "$diagnostic_setting" /bin/bash ./run.sh \
  --profile precog-ultimate --build-only --run-first uzip || exit 1

public_version="$(python3 build_support/update_build_version.py --current)"
public_version="${public_version%[A-Za-z]}"
d81="$(ls -t "Releases/$public_version/precog-ultimate/"*.d81 | head -1)"
if [ -z "$d81" ] || [ ! -f "$d81" ]; then
  echo "diagnostic Ultimate D81 not found" >> "$log"; exit 1
fi
cp "$d81" "$out_dir/${probe_lower}-diagnostic.d81"
cp bin/uzip.prg "$out_dir/uzip-diagnostic.prg"
shasum -a 256 "$out_dir/${probe_lower}-diagnostic.d81" \
  "$out_dir/uzip-diagnostic.prg" > "$out_dir/SHA256.txt"

wait_for_url "C64U FTP" "ftp://anonymous:anonymous%40@${host}/USB1/" || exit 2
existing="$out_dir/existing-roots.txt"
if curl --fail --silent --show-error --max-time 30 --list-only \
  "ftp://anonymous:anonymous%40@${host}/${remote_parent}/" > "$existing" 2>>"$log"; then
  if grep -Fqx "$run_id" "$existing"; then
    echo "fresh owned root already exists; refusing mutation" >> "$log"; exit 2
  fi
fi
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/owner.marker" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" || exit 2
for name in EMPTY REPEAT RANDOM CROSS; do
  run curl --fail --silent --show-error --ftp-create-dirs --max-time 180 \
    -T "$fixtures/${name}.BIN" \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/${name}.BIN" || exit 2
done
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.readback" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.readback" || exit 2
wait_for_url "C64U REST" "http://${host}/v1/drives" || exit 2

speed_before="$out_dir/speed-before.json"
run curl --fail --silent --show-error --max-time 15 \
  "http://${host}/v1/configs/U64%20Specific%20Settings" \
  -o "$speed_before" || exit 2
IFS=$'\t' read -r original_turbo_encoded original_speed_encoded < <(
  python3 - "$speed_before" <<'PY'
import json
import sys
import urllib.parse

settings = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
print(urllib.parse.quote(str(settings["Turbo Control"]), safe=""),
      urllib.parse.quote(str(settings["CPU Speed"]), safe=""), sep="\t")
PY
)
[[ -n "$original_turbo_encoded" && -n "$original_speed_encoded" ]] || exit 2
speed_saved=1

# Blank REU state makes package/scratch ownership deterministic. This is a
# physical ReadyOS boot helper and never launches an emulator. Ultimate 3.14
# can leave only the attachment form of runners:run_prg returning 404; the
# helper still leaves its generated clear PRG for a safely scoped path launch.
if ! run env C64U_HOST="$host" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
  "$out_dir/${probe_lower}-diagnostic.d81" "${probe_upper}-CLEAR.D81" \
  "$out_dir/reu-clear"; then
  clear_prg="$out_dir/reu-clear/tmp/clear_reu.prg"
  if [[ ! -f "$clear_prg" ]]; then
    echo "REU-clear helper failed without producing its clear PRG" >> "$log"
    exit 2
  fi
  echo "attached REU-clear launch unavailable; trying owned-path fallback" >> "$log"
  # Revalidate the exact fresh owner marker before and after the only fallback
  # mutation. The uploaded helper cannot replace content outside this run root.
  run curl --fail --silent --show-error --max-time 30 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
    -o "$out_dir/owner.pre-clear-path-launch" || exit 2
  cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-clear-path-launch" || exit 2
  run curl --fail --silent --show-error --ftp-create-dirs --max-time 120 \
    -T "$clear_prg" \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/clear_reu.prg" || exit 2
  run curl --fail --silent --show-error --max-time 30 -X PUT \
    "http://${host}/v1/runners:run_prg?file=/${remote_root}/clear_reu.prg" || exit 2
  sleep "${READYOS_CLEAR_REU_WAIT_S:-20}"
  run curl --fail --silent --show-error --max-time 30 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
    -o "$out_dir/owner.post-clear-path-launch" || exit 2
  cmp -s "$fixtures/owner.marker" "$out_dir/owner.post-clear-path-launch" || exit 2
fi

# The firmware attachment buffer can remain wedged after an earlier plan even
# though ordinary FTP and body-free REST are healthy. Stage the diagnostic D81
# only below this owner-verified fresh root and pre-mount it; the harness launch
# then performs no attachment upload and cannot overwrite another storage item.
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.pre-d81-upload" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-d81-upload" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 300 \
  -T "$out_dir/${probe_lower}-diagnostic.d81" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/${probe_upper}.D81" || exit 2
run curl --fail --silent --show-error --max-time 30 -X PUT \
  "http://${host}/v1/configs/Drive%20A%20Settings/Drive?value=Enabled" \
  -o "$out_dir/drive-enable.json" || exit 2
require_no_rest_errors "$out_dir/drive-enable.json" || exit 2
run curl --fail --silent --show-error --max-time 30 -X PUT \
  "http://${host}/v1/configs/Drive%20A%20Settings/Drive%20Type?value=1581" \
  -o "$out_dir/drive-type.json" || exit 2
require_no_rest_errors "$out_dir/drive-type.json" || exit 2
run curl --fail --silent --show-error --max-time 30 -X PUT \
  "http://${host}/v1/configs/Drive%20A%20Settings/Drive%20Bus%20ID?value=8" \
  -o "$out_dir/drive-bus.json" || exit 2
require_no_rest_errors "$out_dir/drive-bus.json" || exit 2
encoded_remote_root="${remote_root//\//%2F}"
run curl --fail --silent --show-error --max-time 60 -X PUT \
  "http://${host}/v1/drives/a:mount?image=%2F${encoded_remote_root}%2F${probe_upper}.D81&type=d81&mode=readwrite" \
  -o "$out_dir/drive-mount.json" || exit 2
require_no_rest_errors "$out_dir/drive-mount.json" || exit 2
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.post-d81-mount" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.post-d81-mount" || exit 2

sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#USB1/READYOS_UZIP_TEST/XUZDEFLATE-RUN#${remote_root}#g" \
  -e "s#stream_port: 12300#stream_port: ${stream_port}#g" \
  build_support/xuzdeflate_boot_ultimate.generated.yaml > "$boot_plan"
sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#USB1/READYOS_UZIP_TEST/XUZDEFLATE-RUN#${remote_root}#g" \
  -e "s#stream_port: 12300#stream_port: ${stream_port}#g" \
  -e "s#post_delay_s: 600.0#post_delay_s: ${quiet_s}.0#g" \
  -e "s#XUZDEFLATE FINISHED PASS#${pass_screen}#g" \
  build_support/xuzdeflate_ultimate.generated.yaml > "$codec_plan"

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$boot_plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate boot-to-prompt plan failed" >> "$log"
  exit 2
fi

cd "$readyos_root" || exit 1
# Apply speed only after ReadyOS has reached the diagnostic prompt. Resetting
# after these writes can leave the config value intact while the live core is
# back at startup speed. Body-free per-setting PUTs also bypass the firmware's
# wedged attachment buffer.
run curl --fail --silent --show-error --max-time 30 -X PUT \
  "http://${host}/v1/configs/U64%20Specific%20Settings/Turbo%20Control?value=Manual" \
  -o "$out_dir/speed-turbo-manual.json" || exit 2
require_no_rest_errors "$out_dir/speed-turbo-manual.json" || exit 2
speed_encoded="$(printf '%2d' "$speed" | sed 's/ /%20/g')"
run curl --fail --silent --show-error --max-time 30 -X PUT \
  "http://${host}/v1/configs/U64%20Specific%20Settings/CPU%20Speed?value=${speed_encoded}" \
  -o "$out_dir/speed-value.json" || exit 2
require_no_rest_errors "$out_dir/speed-value.json" || exit 2
expected_turbo="Manual"
if [[ "$speed" == "1" ]]; then
  run curl --fail --silent --show-error --max-time 30 -X PUT \
    "http://${host}/v1/configs/U64%20Specific%20Settings/Turbo%20Control?value=Off" \
    -o "$out_dir/speed-turbo-off.json" || exit 2
  require_no_rest_errors "$out_dir/speed-turbo-off.json" || exit 2
  expected_turbo="Off"
fi
sleep 2
run curl --fail --silent --show-error --max-time 30 \
  "http://${host}/v1/configs/U64%20Specific%20Settings" \
  -o "$out_dir/speed-pre-codec.json" || exit 2
python3 - "$out_dir/speed-pre-codec.json" "$speed" "$expected_turbo" >> "$log" 2>&1 <<'PY' || exit 2
import json
import sys

settings = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
actual_speed = str(settings["CPU Speed"]).strip()
actual_turbo = str(settings["Turbo Control"])
if actual_speed != sys.argv[2] or actual_turbo != sys.argv[3]:
    raise SystemExit(
        f"live CPU mismatch before codec: turbo={actual_turbo!r} "
        f"speed={actual_speed!r}; expected turbo={sys.argv[3]!r} "
        f"speed={sys.argv[2]!r}"
    )
print(f"live CPU confirmed before codec: turbo={actual_turbo} speed={actual_speed} MHz")
PY

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$codec_plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate plan reported failure; collecting evidence" >> "$log"
fi

cd "$readyos_root" || exit 1
run curl --fail --silent --show-error --max-time 15 \
  "http://${host}/v1/configs/U64%20Specific%20Settings" \
  -o "$out_dir/speed-live.json" || exit 2
python3 - "$out_dir/speed-live.json" "$speed" >> "$log" 2>&1 <<'PY' || exit 2
import json
import sys

settings = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
actual = str(settings["CPU Speed"]).strip()
if actual != sys.argv[2]:
    raise SystemExit(f"live CPU speed {actual!r}, expected {sys.argv[2]!r}")
print(f"live CPU confirmed after codec: {actual} MHz")
PY
run python3 build_support/analyze_xuzdeflate_run.py "$log" "$fixtures" \
  --probe-only --json-output "$out_dir/probe-result.json" || exit 4
if [[ "$archive_mode" == "zipmulti" ]]; then
  run curl --fail --silent --show-error --max-time 300 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/MULTI.ZIP" \
    -o "$outputs/MULTI.ZIP" || exit 3
else
  for name in EMPTY REPEAT RANDOM CROSS; do
    run curl --fail --silent --show-error --max-time 180 \
      "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/${name}.${output_ext}" \
      -o "$outputs/${name}.${output_ext}" || exit 3
  done
fi
if [[ "$archive_mode" == "raw" ]]; then
  run python3 build_support/analyze_xuzdeflate_run.py "$log" "$fixtures" \
    --outputs "$outputs" --json-output "$out_dir/result.json" || exit 4
elif [[ "$archive_mode" == "zip8" ]]; then
  run python3 build_support/analyze_xuzzip8_outputs.py "$fixtures" "$outputs" \
    --json-output "$out_dir/zip-result.json" || exit 4
  run python3 - "$out_dir/probe-result.json" "$out_dir/zip-result.json" \
    "$out_dir/result.json" <<'PY' || exit 4
import json
from pathlib import Path
import sys

probe = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
probe.update(json.loads(Path(sys.argv[2]).read_text(encoding="utf-8")))
Path(sys.argv[3]).write_text(
    json.dumps(probe, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)
PY
else
  run python3 build_support/analyze_xuzmulti_outputs.py "$fixtures" \
    "$outputs/MULTI.ZIP" --json-output "$out_dir/zip-result.json" || exit 4
  run python3 - "$out_dir/probe-result.json" "$out_dir/zip-result.json" \
    "$out_dir/result.json" <<'PY' || exit 4
import json
from pathlib import Path
import sys

probe = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
probe.update(json.loads(Path(sys.argv[2]).read_text(encoding="utf-8")))
Path(sys.argv[3]).write_text(
    json.dumps(probe, indent=2, sort_keys=True) + "\n", encoding="utf-8"
)
PY
fi
echo "${probe_upper} PHYSICAL PASS ${speed}MHz /$remote_root" | \
  tee "$out_dir/PASS.txt" >> "$log"
exit 0
