#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZINFLATE_SPEED_MHZ:-16}"
stream_port="${XUZINFLATE_STREAM_PORT:-12100}"
log="${1:-/tmp/xuzinflate_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/xuzinflate_c64u_${speed}mhz.status}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
run_id="XUZINFLATE-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${XUZINFLATE_OUT_DIR:-$readyos_root/logs/xuzinflate/$run_id}"
plan="$out_dir/xuzinflate-plan.yaml"
fixtures="$out_dir/fixtures"
outputs="$out_dir/outputs"
quiet_s="${XUZINFLATE_QUIET_S:-}"

case "$speed" in 1|16|64) ;; *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;; esac
if [[ -z "$quiet_s" ]]; then
  case "$speed" in
    1) quiet_s=900 ;;
    16|64) quiet_s=180 ;;
  esac
fi
case "$quiet_s" in *[!0-9]*|'') echo "quiet seconds must be an integer" >&2; exit 64 ;; esac
case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZINFLATE-*) ;;
  *) echo "refusing non-owned xuzinflate root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$fixtures" "$outputs"
rm -f "$log" "$status_file"
speed_saved=0
original_turbo_encoded=""
original_speed_encoded=""
finish() {
  rc=$?
  trap - EXIT
  if [[ "$speed_saved" == "1" ]]; then
    restore_rc=0
    # Restore in a safe order: enter Manual, restore the saved numeric speed,
    # then restore the exact original control mode. These per-setting PUTs do
    # not use the firmware attachment buffer.
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

{
  date
  echo "Physical C64 Ultimate xuzinflate"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
} > "$log"

cd "$readyos_root" || exit 1
run python3 build_support/build_xuzinflate_fixture.py "$fixtures" --run-id "$run_id" || exit 1
run env XUZINFLATE_RUN_ID="$run_id" XUZINFLATE_VOLUME=USB1 \
  XUZINFLATE_FIXTURE_DIR="$fixtures" /bin/bash probes/xuzinflate/build.sh || exit 1
run cp "$readyos_root/build/xuzinflate/xuzinflate.prg" "$out_dir/xuzinflate.prg" || exit 1
run shasum -a 256 "$out_dir/xuzinflate.prg" || exit 1
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
for name in EMPTY STORED FIXED DYNAMIC TRUNC TRAIL BADTYPE BADSTORED \
  BADDIST BADLENGTH BADRSVDIST BADREPEAT BADTREE; do
  run curl --fail --silent --show-error --ftp-create-dirs --max-time 120 \
    -T "$fixtures/${name}.RAW" \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/${name}.RAW" || exit 2
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

# A prior standalone probe can still own Ultimate DOS handles or leave the
# cartridge/UCI core wedged while sitting in its bounded FINISHED loop. A soft
# C64 reset does not clear that state, so use the same Ultimate-core reboot
# recovery already proven by the repository's UCITEST and ReadyIRC suites.
run curl --fail --silent --show-error --max-time 10 -X PUT \
  "http://${host}/v1/machine:reboot" || exit 2
sleep 8

# Apply the live turbo state after reboot. Rebooting after these writes can
# leave the configuration value intact while the running core is back at its
# startup speed, which would make a nominal 16/64 MHz result dishonest.
# Aggregate JSON POSTs also share the firmware's currently wedged attachment
# buffer, so keep using body-free per-setting PUTs.
run curl --fail --silent --show-error --max-time 15 -X PUT \
  "http://${host}/v1/configs/U64%20Specific%20Settings/Turbo%20Control?value=Manual" || exit 2
run curl --fail --silent --show-error --max-time 15 -X PUT \
  "http://${host}/v1/configs/U64%20Specific%20Settings/CPU%20Speed?value=$(printf '%2d' "$speed" | sed 's/ /%20/g')" || exit 2
expected_turbo="Manual"
if [[ "$speed" == "1" ]]; then
  run curl --fail --silent --show-error --max-time 15 -X PUT \
    "http://${host}/v1/configs/U64%20Specific%20Settings/Turbo%20Control?value=Off" || exit 2
  expected_turbo="Off"
fi
sleep 2
run curl --fail --silent --show-error --max-time 15 \
  "http://${host}/v1/configs/U64%20Specific%20Settings" \
  -o "$out_dir/speed-live.json" || exit 2
python3 - "$out_dir/speed-live.json" "$speed" "$expected_turbo" >> "$log" 2>&1 <<'PY' || exit 2
import json
import sys

settings = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
actual_speed = str(settings["CPU Speed"]).strip()
actual_turbo = str(settings["Turbo Control"])
if actual_speed != sys.argv[2] or actual_turbo != sys.argv[3]:
    raise SystemExit(
        f"live CPU mismatch: turbo={actual_turbo!r} speed={actual_speed!r}; "
        f"expected turbo={sys.argv[3]!r} speed={sys.argv[2]!r}"
    )
print(f"live CPU confirmed: turbo={actual_turbo} speed={actual_speed} MHz")
PY

launched=0
# Ultimate firmware 3.14 may transiently return 404 from runners:run_prg after
# another automation plan. Retry finitely while the Terminal-owned process has
# exclusive responsibility for the physical-hardware REST launch.
for attempt in $(seq 1 10); do
  echo "+ curl runners:run_prg attempt=${attempt}" >> "$log"
  if curl --fail --silent --show-error --max-time 30 \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@${out_dir}/xuzinflate.prg" \
    "http://${host}/v1/runners:run_prg" >> "$log" 2>&1; then
    launched=1
    break
  fi
  sleep 2
done
if [[ "$launched" != "1" ]]; then
  echo "attached runners:run_prg unavailable after 10 attempts; trying owned-path fallback" >> "$log"
  # Firmware can leave only the REST attachment handoff wedged. Revalidate the
  # exact owner marker, upload beneath that fresh root, and launch by Ultimate
  # path; this never addresses or replaces an unrelated storage object.
  run curl --fail --silent --show-error --max-time 30 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
    -o "$out_dir/owner.pre-path-launch" || exit 2
  cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-path-launch" || exit 2
  run curl --fail --silent --show-error --ftp-create-dirs --max-time 120 \
    -T "${out_dir}/xuzinflate.prg" \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/xuzinflate.prg" || exit 2
  run curl --fail --silent --show-error --max-time 30 -X PUT \
    "http://${host}/v1/runners:run_prg?file=/${remote_root}/xuzinflate.prg" || exit 2
  run curl --fail --silent --show-error --max-time 30 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
    -o "$out_dir/owner.post-path-launch" || exit 2
  cmp -s "$fixtures/owner.marker" "$out_dir/owner.post-path-launch" || exit 2
  launched=1
fi
sleep 2

sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#remote_root: USB1/READYOS_UZIP_TEST/XUZINFLATE-RUN#remote_root: ${remote_root}#g" \
  -e "s#stream_port: 12100#stream_port: ${stream_port}#g" \
  -e "s#post_delay_s: 600.0#post_delay_s: ${quiet_s}.0#g" \
  build_support/xuzinflate_ultimate.generated.yaml > "$plan"

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate plan reported failure; collecting evidence" >> "$log"
fi

cd "$readyos_root" || exit 1
run python3 build_support/analyze_xuzinflate_run.py "$log" "$fixtures" \
  --probe-only --json-output "$out_dir/probe-result.json" || exit 4
for name in EMPTY STORED FIXED DYNAMIC; do
  run curl --fail --silent --show-error --max-time 180 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/${name}.BIN" \
    -o "$outputs/${name}.BIN" || exit 3
done
run python3 build_support/analyze_xuzinflate_run.py "$log" "$fixtures" \
  --outputs "$outputs" --json-output "$out_dir/result.json" || exit 4
echo "XUZINFLATE PHYSICAL PASS ${speed}MHz /$remote_root" | tee "$out_dir/PASS.txt" >> "$log"
exit 0
