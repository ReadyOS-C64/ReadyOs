#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
speed="${UZIP_WORKFLOW_SPEED_MHZ:-16}"
stream_port="${UZIP_WORKFLOW_STREAM_PORT:-12258}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
run_id="UZIPFLOW-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${UZIP_WORKFLOW_OUT_DIR:-$readyos_root/logs/uzip-workflow/$run_id}"
fixtures="$out_dir/fixtures"
log="${1:-/tmp/uzip_workflow_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/uzip_workflow_c64u_${speed}mhz.status}"
boot_plan="$out_dir/uzip-workflow-boot.yaml"
workflow_plan="$out_dir/uzip-workflow-run.yaml"
workflow_config="$out_dir/precog-ultimate-workflow.ini"
expected_uzip_sha256="${UZIP_WORKFLOW_EXPECTED_UZIP_SHA256:-}"
expected_uzpack_sha256="${UZIP_WORKFLOW_EXPECTED_UZPACK_SHA256:-}"

case "$speed" in
  1|16|64) ;;
  *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;;
esac

case "$remote_root" in
  USB1/READYOS_UZIP_TEST/UZIPFLOW-*) ;;
  *) echo "refusing non-owned uZIP workflow root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$fixtures"
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
import json
import sys
reply = json.load(open(sys.argv[1], encoding="utf-8"))
if reply.get("errors", []):
    raise SystemExit(f"Ultimate REST errors: {reply['errors']!r}")
PY
}

{
  date
  echo "Physical C64 Ultimate complete ReadyOS uZIP UI workflow"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
  echo "VICE use: forbidden"
} > "$log"

cd "$readyos_root" || exit 1
run python3 build_support/build_uzip_workflow_fixture.py "$fixtures" \
  --run-id "$run_id" || exit 1
run python3 build_support/verify_uci_protocol_contract.py || exit 1
run python3 - cfg/profiles/precog-ultimate.ini "$workflow_config" \
  "/${remote_root}/UZIPFLOW.D81" <<'PY'
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
image_path = sys.argv[3].lower()
lines = source.read_text(encoding="utf-8").splitlines()
output = []
replaced = False
for line in lines:
    if line.startswith("c64u_image_path="):
        output.append(f"c64u_image_path={image_path}")
        replaced = True
    else:
        output.append(line)
if not replaced:
    raise SystemExit("Ultimate workflow profile lacks c64u_image_path")
target.write_text("\n".join(output) + "\n", encoding="utf-8")
PY
run /bin/bash ./run.sh --profile precog-ultimate --build-only \
  --config "$workflow_config" || exit 1
run python3 build_support/verify_uzip_contract.py || exit 1

public_version="$(python3 build_support/update_build_version.py --current)"
public_version="${public_version%[A-Za-z]}"
d81="$(ls -t "Releases/$public_version/precog-ultimate/"*.d81 | head -1)"
if [[ -z "$d81" || ! -f "$d81" ]]; then
  echo "uZIP workflow Ultimate D81 not found" >> "$log"; exit 1
fi
cp "$d81" "$out_dir/uzip-workflow.d81"
cp bin/uzip.prg "$out_dir/uzip.prg"
cp bin/uzpack.prg "$out_dir/uzpack.prg"
shasum -a 256 "$out_dir/uzip-workflow.d81" "$out_dir/uzip.prg" \
  "$out_dir/uzpack.prg" > "$out_dir/SHA256.txt"
actual_uzip_sha256="$(shasum -a 256 "$out_dir/uzip.prg" | awk '{print $1}')"
actual_uzpack_sha256="$(shasum -a 256 "$out_dir/uzpack.prg" | awk '{print $1}')"
if [[ -n "$expected_uzip_sha256" && "$actual_uzip_sha256" != "$expected_uzip_sha256" ]]; then
  echo "uzip.prg differs from frozen matrix member: $actual_uzip_sha256" >> "$log"
  exit 1
fi
if [[ -n "$expected_uzpack_sha256" && "$actual_uzpack_sha256" != "$expected_uzpack_sha256" ]]; then
  echo "uzpack.prg differs from frozen matrix member: $actual_uzpack_sha256" >> "$log"
  exit 1
fi

ftp_base="ftp://anonymous:anonymous%40@${host}"
wait_for_url "C64U FTP" "$ftp_base/USB1/" || exit 2
wait_for_url "C64U REST" "http://${host}/v1/drives" || exit 2

existing="$out_dir/existing-roots.txt"
if curl --fail --silent --show-error --max-time 30 --list-only \
  "$ftp_base/${remote_parent}/" > "$existing" 2>>"$log"; then
  if grep -Fqx "$run_id" "$existing"; then
    echo "fresh owned root already exists; refusing mutation" >> "$log"; exit 2
  fi
fi
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/owner.marker" \
  "$ftp_base/${remote_root}/.READYOS-UZIP-OWNER" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/LOOSE.BIN" \
  "$ftp_base/${remote_root}/SOURCE/LOOSE.BIN" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/TREE-ROOT.TXT" \
  "$ftp_base/${remote_root}/SOURCE/TREE/ROOT.TXT" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/TREE-DEEP.BIN" \
  "$ftp_base/${remote_root}/SOURCE/TREE/NEST/DEEP.BIN" || exit 2
run curl --fail --silent --show-error --max-time 60 \
  --quote "MKD /${remote_root}/OUT" "$ftp_base/" || exit 2
run curl --fail --silent --show-error --max-time 60 \
  --quote "MKD /${remote_root}/DEST" "$ftp_base/" || exit 2

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
if [[ -z "$original_turbo_encoded" || -z "$original_speed_encoded" ]]; then
  echo "could not capture CPU settings" >> "$log"; exit 2
fi
speed_saved=1

if ! run env C64U_HOST="$host" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
  "$out_dir/uzip-workflow.d81" UZIPFLOW-CLEAR.D81 \
  "$out_dir/reu-clear"; then
  clear_prg="$out_dir/reu-clear/tmp/clear_reu.prg"
  [[ -f "$clear_prg" ]] || {
    echo "REU clear PRG missing after attachment launch failure" >> "$log"
    exit 2
  }
  echo "attached REU-clear launch unavailable; using owned-path fallback" >> "$log"
  run curl --fail --silent --show-error --max-time 30 \
    "$ftp_base/${remote_root}/.READYOS-UZIP-OWNER" \
    -o "$out_dir/owner.pre-clear-fallback" || exit 2
  cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-clear-fallback" || exit 2
  run curl --fail --silent --show-error --ftp-create-dirs --max-time 120 \
    -T "$clear_prg" "$ftp_base/${remote_root}/clear_reu.prg" || exit 2
  run curl --fail --silent --show-error --max-time 30 -X PUT \
    "http://${host}/v1/runners:run_prg?file=/${remote_root}/clear_reu.prg" || exit 2
  sleep "${READYOS_CLEAR_REU_WAIT_S:-20}"
fi

run curl --fail --silent --show-error --max-time 30 \
  "$ftp_base/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.pre-upload" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.pre-upload" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 300 \
  -T "$out_dir/uzip-workflow.d81" \
  "$ftp_base/${remote_root}/UZIPFLOW.D81" || exit 2

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
  "http://${host}/v1/drives/a:mount?image=%2F${encoded_remote_root}%2FUZIPFLOW.D81&type=d81&mode=readwrite" \
  -o "$out_dir/drive-mount.json" || exit 2
require_no_rest_errors "$out_dir/drive-mount.json" || exit 2

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
  -o "$out_dir/speed-live.json" || exit 2
python3 - "$out_dir/speed-live.json" "$speed" "$expected_turbo" >> "$log" 2>&1 <<'PY' || exit 2
import json, sys
settings = json.load(open(sys.argv[1], encoding="utf-8"))["U64 Specific Settings"]
if (str(settings["CPU Speed"]).strip() != sys.argv[2] or
        str(settings["Turbo Control"]) != sys.argv[3]):
    raise SystemExit(
        f"live CPU mismatch: {settings['Turbo Control']!r}/{settings['CPU Speed']!r}"
    )
print(f"live CPU confirmed: turbo={settings['Turbo Control']} speed={settings['CPU Speed']} MHz")
PY

run python3 build_support/build_uzip_workflow_plans.py \
  --host "$host" --remote-root "$remote_root" --stream-port "$stream_port" \
  --speed-mhz "$speed" \
  --boot-output "$boot_plan" --workflow-output "$workflow_plan" || exit 1

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$boot_plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate ReadyOS/uZIP boot plan failed" >> "$log"; exit 3
fi
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$workflow_plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate complete uZIP workflow plan failed" >> "$log"; exit 4
fi

cd "$readyos_root" || exit 1
run curl --fail --silent --show-error --max-time 90 \
  "$ftp_base/${remote_root}/OUT/archive.zip" \
  -o "$out_dir/archive.zip" || exit 5
run curl --fail --silent --show-error --max-time 90 \
  "$ftp_base/${remote_root}/OUT/store.zip" \
  -o "$out_dir/store.zip" || exit 5
for spec in \
  "LOOSE.BIN:LOOSE.BIN" \
  "TREE-ROOT.TXT:TREE/ROOT.TXT" \
  "TREE-DEEP.BIN:TREE/NEST/DEEP.BIN"; do
  local_name="${spec%%:*}"
  remote_name="${spec#*:}"
  run curl --fail --silent --show-error --max-time 90 \
    "$ftp_base/${remote_root}/DEST/${remote_name}" \
    -o "$out_dir/extracted-${local_name}" || exit 5
  run curl --fail --silent --show-error --max-time 90 \
    "$ftp_base/${remote_root}/SOURCE/${remote_name}" \
    -o "$out_dir/source-${local_name}" || exit 5
done
run curl --fail --silent --show-error --max-time 60 --list-only \
  "$ftp_base/${remote_root}/OUT/" -o "$out_dir/output-list.txt" || exit 5
run curl --fail --silent --show-error --max-time 60 --list-only \
  "$ftp_base/${remote_root}/DEST/" -o "$out_dir/destination-list.txt" || exit 5
run curl --fail --silent --show-error --max-time 60 --list-only \
  "$ftp_base/${remote_root}/DEST/TREE/" -o "$out_dir/destination-tree-list.txt" || exit 5
run curl --fail --silent --show-error --max-time 60 --list-only \
  "$ftp_base/${remote_root}/DEST/TREE/NEST/" -o "$out_dir/destination-nest-list.txt" || exit 5

run python3 build_support/analyze_uzip_workflow_outputs.py \
  --fixture-dir "$fixtures" --results-dir "$out_dir" \
  --archive "$out_dir/archive.zip" \
  --store-archive "$out_dir/store.zip" \
  --output-list "$out_dir/output-list.txt" \
  --destination-list "$out_dir/destination-list.txt" \
  --destination-tree-list "$out_dir/destination-tree-list.txt" \
  --destination-nest-list "$out_dir/destination-nest-list.txt" || exit 5

echo "UZIP COMPLETE WORKFLOW PHYSICAL PASS ${speed}MHz /$remote_root" | \
  tee "$out_dir/PASS.txt" >> "$log"
exit 0
