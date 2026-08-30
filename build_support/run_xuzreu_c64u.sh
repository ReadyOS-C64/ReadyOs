#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZREU_SPEED_MHZ:-16}"
stream_port="${XUZREU_STREAM_PORT:-12200}"
log="${1:-/tmp/xuzreu_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/xuzreu_c64u_${speed}mhz.status}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
run_id="XUZREU-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${XUZREU_OUT_DIR:-$readyos_root/logs/xuzreu/$run_id}"
fixtures="$out_dir/fixtures"
plan="$out_dir/xuzreu-plan.yaml"
downloaded="$out_dir/RESULT.BIN"

case "$speed" in 1|16|64) ;; *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;; esac
case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZREU-*) ;;
  *) echo "refusing non-owned xuzreu root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$fixtures" "$readyos_root/obj/xuzreu"
rm -f "$log" "$status_file"
finish() { rc=$?; echo "$rc" > "$status_file"; echo "EXIT $rc" >> "$log"; exit "$rc"; }
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
  echo "Physical C64 Ultimate xuzreu full-ReadyOS diagnostic"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
  echo "Emulator use: forbidden"
} > "$log"

cd "$readyos_root" || exit 1
run python3 build_support/build_xuzreu_fixture.py "$fixtures" \
  --run-id "$run_id" --header obj/xuzreu/xuzreu_config.h || exit 1
run python3 build_support/verify_uci_protocol_contract.py || exit 1
run env UZIP_DIAGNOSTIC=1 /bin/bash ./run.sh --profile precog-ultimate \
  --build-only --run-first uzip || exit 1

public_version="$(python3 build_support/update_build_version.py --current)"
public_version="${public_version%[A-Za-z]}"
d81="$(ls -t "Releases/$public_version/precog-ultimate/"*.d81 | head -1)"
if [ -z "$d81" ] || [ ! -f "$d81" ]; then
  echo "diagnostic Ultimate D81 not found" >> "$log"; exit 1
fi
cp "$d81" "$out_dir/xuzreu-diagnostic.d81"
cp bin/uzip.prg "$out_dir/uzip-diagnostic.prg"
shasum -a 256 "$out_dir/xuzreu-diagnostic.d81" \
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
run curl --fail --silent --show-error --ftp-create-dirs --max-time 120 \
  -T "$fixtures/SOURCE.BIN" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/SOURCE.BIN" || exit 2
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.readback" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.readback" || exit 2
wait_for_url "C64U REST" "http://${host}/v1/drives" || exit 2

# A blank REU makes the expected snapshot/package allocation order
# deterministic. This is test setup only; all direct-transfer assertions run
# inside the physical ReadyOS app.
run env C64U_HOST="$host" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
  /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
  "$out_dir/xuzreu-diagnostic.d81" XUZREU-CLEAR.D81 "$out_dir/reu-clear" || exit 2

sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#USB1/READYOS_UZIP_TEST/XUZREU-RUN#${remote_root}#g" \
  -e "s#XUZREU-DIAGNOSTIC-D81#${out_dir}/xuzreu-diagnostic.d81#g" \
  -e "s#stream_port: 12200#stream_port: ${stream_port}#g" \
  -e "s#mhz: 16#mhz: ${speed}#g" \
  build_support/xuzreu_ultimate.generated.yaml > "$plan"

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate plan reported failure; collecting evidence" >> "$log"
fi

cd "$readyos_root" || exit 1
run python3 build_support/analyze_xuzreu_run.py "$log" "$fixtures" \
  --core-only --json-output "$out_dir/core-result.json" || exit 4
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/RESULT.BIN" \
  -o "$downloaded" || exit 3
run python3 build_support/analyze_xuzreu_run.py "$log" "$fixtures" \
  --downloaded-output "$downloaded" --json-output "$out_dir/result.json" || exit 4
echo "XUZREU PHYSICAL PASS ${speed}MHz /$remote_root" | tee "$out_dir/PASS.txt" >> "$log"
exit 0
