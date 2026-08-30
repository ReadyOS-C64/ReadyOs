#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZEXTRACT_SPEED_MHZ:-16}"
extract_wait_s=180
stream_port="${XUZEXTRACT_STREAM_PORT:-12240}"
log="${1:-/tmp/xuzextract_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/xuzextract_c64u_${speed}mhz.status}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
run_id="XUZEXTRACT-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${XUZEXTRACT_OUT_DIR:-$readyos_root/logs/xuzextract/$run_id}"
fixtures="$out_dir/fixtures"
downloads="$out_dir/downloads"
boot_plan="$out_dir/xuzextract-boot-plan.yaml"
extract_plan="$out_dir/xuzextract-plan.yaml"

case "$speed" in
  1) extract_wait_s=900 ;;
  16|64) ;;
  *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;;
esac
case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZEXTRACT-*) ;;
  *) echo "refusing non-owned xuzextract root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$fixtures" "$downloads" "$readyos_root/obj/xuzextract"
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
import json, sys
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
import json, sys
reply = json.load(open(sys.argv[1], encoding="utf-8"))
if reply.get("errors", []):
    raise SystemExit(f"Ultimate REST errors: {reply['errors']!r}")
PY
}

{
  date
  echo "Physical C64 Ultimate xuzextract transaction diagnostic"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
  echo "VICE use: forbidden"
} > "$log"

cd "$readyos_root" || exit 1
run python3 build_support/build_xuzextract_fixture.py "$fixtures" \
  --run-id "$run_id" --header obj/xuzextract/xuzextract_config.h || exit 1
run python3 build_support/verify_uci_protocol_contract.py || exit 1
# Freeze a valid launcher resource before linking the self-seeded diagnostic;
# a diagnostic map does not use the production package's fixed load offsets.
run /bin/bash ./run.sh --profile precog-ultimate --build-only || exit 1
run env UZIP_EXTRACT_DIAGNOSTIC=1 /bin/bash ./run.sh \
  --profile precog-ultimate --build-only --run-first uzip || exit 1
run python3 build_support/verify_uzip_contract.py || exit 1

public_version="$(python3 build_support/update_build_version.py --current)"
public_version="${public_version%[A-Za-z]}"
d81="$(ls -t "Releases/$public_version/precog-ultimate/"*.d81 | head -1)"
if [[ -z "$d81" || ! -f "$d81" ]]; then
  echo "diagnostic Ultimate D81 not found" >> "$log"; exit 1
fi
cp "$d81" "$out_dir/xuzextract-diagnostic.d81"
cp bin/uzip.prg "$out_dir/uzip-diagnostic.prg"
cp bin/uzpack.prg "$out_dir/uzpack-production.prg"
shasum -a 256 "$out_dir/xuzextract-diagnostic.d81" \
  "$out_dir/uzip-diagnostic.prg" "$out_dir/uzpack-production.prg" \
  > "$out_dir/SHA256.txt"

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
  -T "$fixtures/GOOD.ZIP" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/GOOD.ZIP" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 180 \
  -T "$fixtures/BADCRC.ZIP" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/BADCRC.ZIP" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/existing.keep" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUT/EXISTING/KEEP.BIN" || exit 2
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.readback" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.readback" || exit 2

wait_for_url "C64U REST" "http://${host}/v1/drives" || exit 2
run curl --fail --silent --show-error --max-time 15 \
  "http://${host}/v1/configs/U64%20Specific%20Settings" -o "$speed_before" || exit 2
IFS=$'\t' read -r original_turbo_encoded original_speed_encoded < <(
  python3 - "$speed_before" <<'PY'
import json, sys, urllib.parse
s = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
print(urllib.parse.quote(str(s["Turbo Control"]), safe=""),
      urllib.parse.quote(str(s["CPU Speed"]), safe=""), sep="\t")
PY
)
[[ -n "$original_turbo_encoded" && -n "$original_speed_encoded" ]] || exit 2
speed_saved=1

if ! run env C64U_HOST="$host" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
  "$out_dir/xuzextract-diagnostic.d81" XUZEXTRACT-CLEAR.D81 "$out_dir/reu-clear"; then
  clear_prg="$out_dir/reu-clear/tmp/clear_reu.prg"
  [[ -f "$clear_prg" ]] || { echo "REU clear PRG missing" >> "$log"; exit 2; }
  run curl --fail --silent --show-error --max-time 30 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
    -o "$out_dir/owner.pre-clear" || exit 2
  cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-clear" || exit 2
  run curl --fail --silent --show-error --ftp-create-dirs --max-time 120 \
    -T "$clear_prg" "ftp://anonymous:anonymous%40@${host}/${remote_root}/clear_reu.prg" || exit 2
  run curl --fail --silent --show-error --max-time 30 -X PUT \
    "http://${host}/v1/runners:run_prg?file=/${remote_root}/clear_reu.prg" || exit 2
  sleep "${READYOS_CLEAR_REU_WAIT_S:-20}"
fi

run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.pre-d81" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-d81" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 300 \
  -T "$out_dir/xuzextract-diagnostic.d81" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/XUZEXTRACT.D81" || exit 2
for setting in \
  "Drive=Enabled:drive-enable" \
  "Drive%20Type=1581:drive-type" \
  "Drive%20Bus%20ID=8:drive-bus"; do
  query="${setting%%:*}"; label="${setting#*:}"
  run curl --fail --silent --show-error --max-time 30 -X PUT \
    "http://${host}/v1/configs/Drive%20A%20Settings/${query%%=*}?value=${query#*=}" \
    -o "$out_dir/${label}.json" || exit 2
  require_no_rest_errors "$out_dir/${label}.json" || exit 2
done
encoded_remote_root="${remote_root//\//%2F}"
run curl --fail --silent --show-error --max-time 60 -X PUT \
  "http://${host}/v1/drives/a:mount?image=%2F${encoded_remote_root}%2FXUZEXTRACT.D81&type=d81&mode=readwrite" \
  -o "$out_dir/drive-mount.json" || exit 2
require_no_rest_errors "$out_dir/drive-mount.json" || exit 2

sed -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#USB1/READYOS_UZIP_TEST/XUZEXTRACT-RUN#${remote_root}#g" \
  -e "s#stream_port: 12240#stream_port: ${stream_port}#g" \
  build_support/xuzextract_boot_ultimate.generated.yaml > "$boot_plan"
sed -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#USB1/READYOS_UZIP_TEST/XUZEXTRACT-RUN#${remote_root}#g" \
  -e "s#stream_port: 12240#stream_port: ${stream_port}#g" \
  -e "s#post_delay_s: 180.0#post_delay_s: ${extract_wait_s}.0#g" \
  build_support/xuzextract_ultimate.generated.yaml > "$extract_plan"

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
  -o "$out_dir/speed-pre-extract.json" || exit 2
python3 - "$out_dir/speed-pre-extract.json" "$speed" "$expected_turbo" >> "$log" 2>&1 <<'PY' || exit 2
import json, sys
s = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
if str(s["CPU Speed"]).strip() != sys.argv[2] or str(s["Turbo Control"]) != sys.argv[3]:
    raise SystemExit(f"live CPU mismatch: {s['Turbo Control']!r}/{s['CPU Speed']!r}")
print(f"live CPU confirmed before extraction: turbo={s['Turbo Control']} speed={s['CPU Speed']} MHz")
PY

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$extract_plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate extraction plan reported failure; analyzing evidence" >> "$log"
fi

cd "$readyos_root" || exit 1
run python3 build_support/analyze_xuzextract_run.py "$log" "$fixtures" "$downloads" \
  --result-only --json-output "$out_dir/result-c64.json" || exit 4
for item in \
  "OUT/:out.list" \
  "OUT/NEST/:nest.list" \
  "OUT/NEST/DEEP/:deep.list" \
  "OUT/EXISTING/:existing.list" \
  "OUT/BAD/:bad.list"; do
  path="${item%%:*}"; file="${item#*:}"
  run curl --fail --silent --show-error --max-time 30 --list-only \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/${path}" \
    -o "$downloads/$file" || exit 3
done
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUT/NEST/STORE.BIN" \
  -o "$downloads/STORE.BIN" || exit 3
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUT/NEST/DEEP/DEFLATE.BIN" \
  -o "$downloads/DEFLATE.BIN" || exit 3
run curl --fail --silent --show-error --max-time 60 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUT/EXISTING/KEEP.BIN" \
  -o "$downloads/KEEP.BIN" || exit 3
run python3 build_support/analyze_xuzextract_run.py "$log" "$fixtures" "$downloads" \
  --json-output "$out_dir/result.json" || exit 4
echo "XUZEXTRACT PHYSICAL PASS ${speed}MHz /$remote_root" | \
  tee "$out_dir/PASS.txt" >> "$log"
exit 0
