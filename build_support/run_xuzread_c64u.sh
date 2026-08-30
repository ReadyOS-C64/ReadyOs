#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZREAD_SPEED_MHZ:-16}"
stream_port="${XUZREAD_STREAM_PORT:-12240}"
log="${1:-/tmp/xuzread_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/xuzread_c64u_${speed}mhz.status}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
run_id="XUZREAD-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${XUZREAD_OUT_DIR:-$readyos_root/logs/xuzread/$run_id}"
fixtures="$out_dir/fixtures"
boot_plan="$out_dir/xuzread-boot-plan.yaml"
codec_plan="$out_dir/xuzread-codec-plan.yaml"

case "$speed" in 1|16|64) ;; *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;; esac
case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZREAD-*) ;;
  *) echo "refusing non-owned xuzread root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$fixtures" "$readyos_root/obj/xuzread"
rm -f "$log" "$status_file"
speed_saved=0
original_turbo_encoded=""
original_speed_encoded=""
speed_before="$out_dir/speed-before.json"
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
if reply.get("errors", []):
    raise SystemExit(f"Ultimate REST errors: {reply['errors']!r}")
PY
}

{
  date
  echo "Physical C64 Ultimate xuzread callback-parser diagnostic"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
  echo "VICE use: forbidden"
} > "$log"

cd "$readyos_root" || exit 1
run python3 build_support/build_xuzread_fixture.py "$fixtures" \
  --run-id "$run_id" --header obj/xuzread/xuzread_config.h || exit 1
run python3 build_support/verify_uci_protocol_contract.py || exit 1
run env UZIP_ZIPREAD_DIAGNOSTIC=1 /bin/bash ./run.sh \
  --profile precog-ultimate --build-only --run-first uzip || exit 1
run python3 build_support/verify_uzip_contract.py || exit 1

public_version="$(python3 build_support/update_build_version.py --current)"
public_version="${public_version%[A-Za-z]}"
d81="$(ls -t "Releases/$public_version/precog-ultimate/"*.d81 | head -1)"
if [[ -z "$d81" || ! -f "$d81" ]]; then
  echo "diagnostic Ultimate D81 not found" >> "$log"; exit 1
fi
cp "$d81" "$out_dir/xuzread-diagnostic.d81"
cp bin/uzip.prg "$out_dir/uzip-diagnostic.prg"
shasum -a 256 "$out_dir/xuzread-diagnostic.d81" \
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
run curl --fail --silent --show-error --ftp-create-dirs --max-time 180 \
  -T "$fixtures/ARCHIVE.ZIP" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/ARCHIVE.ZIP" || exit 2
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.readback" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.readback" || exit 2

wait_for_url "C64U REST" "http://${host}/v1/drives" || exit 2
run curl --fail --silent --show-error --max-time 15 \
  "http://${host}/v1/configs/U64%20Specific%20Settings" -o "$speed_before" || exit 2
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
if [[ -z "$original_turbo_encoded" || -z "$original_speed_encoded" ]]; then
  echo "could not capture CPU settings" >> "$log"; exit 2
fi
speed_saved=1

# All REST/FTP setup runs from this Terminal-owned shell. Firmware 3.14 can
# leave only the attachment form of runners:run_prg returning 404, so retain
# the helper PRG and fall back only beneath the exact owner-verified root.
if ! run env C64U_HOST="$host" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
  "$out_dir/xuzread-diagnostic.d81" XUZREAD-CLEAR.D81 "$out_dir/reu-clear"; then
  clear_prg="$out_dir/reu-clear/tmp/clear_reu.prg"
  [[ -f "$clear_prg" ]] || { echo "REU clear PRG missing" >> "$log"; exit 2; }
  echo "attached REU-clear launch unavailable; trying owned-path fallback" >> "$log"
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

# Avoid the same wedged attachment buffer for the diagnostic image. Upload and
# mount only below this run's owner marker, then launch with empty attachments.
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.pre-d81-upload" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-d81-upload" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 300 \
  -T "$out_dir/xuzread-diagnostic.d81" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/XUZREAD.D81" || exit 2
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
  "http://${host}/v1/drives/a:mount?image=%2F${encoded_remote_root}%2FXUZREAD.D81&type=d81&mode=readwrite" \
  -o "$out_dir/drive-mount.json" || exit 2
require_no_rest_errors "$out_dir/drive-mount.json" || exit 2

sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#USB1/READYOS_UZIP_TEST/XUZREAD-RUN#${remote_root}#g" \
  -e "s#stream_port: 12240#stream_port: ${stream_port}#g" \
  build_support/xuzread_boot_ultimate.generated.yaml > "$boot_plan"
sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#USB1/READYOS_UZIP_TEST/XUZREAD-RUN#${remote_root}#g" \
  -e "s#stream_port: 12240#stream_port: ${stream_port}#g" \
  build_support/xuzread_ultimate.generated.yaml > "$codec_plan"

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$boot_plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate boot-to-prompt plan failed" >> "$log"; exit 2
fi

cd "$readyos_root" || exit 1
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
    raise SystemExit(f"live CPU mismatch: {actual_turbo!r}/{actual_speed!r}")
print(f"live CPU confirmed before parser: turbo={actual_turbo} speed={actual_speed} MHz")
PY

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$codec_plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate parser plan reported failure; analyzing result" >> "$log"
fi

cd "$readyos_root" || exit 1
run python3 build_support/analyze_xuzread_run.py "$log" "$fixtures" \
  --json-output "$out_dir/result.json" || exit 4
echo "XUZREAD PHYSICAL PASS ${speed}MHz /$remote_root" | \
  tee "$out_dir/PASS.txt" >> "$log"
exit 0
