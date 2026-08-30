#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZSTORE_SPEED_MHZ:-16}"
stream_port="${XUZSTORE_STREAM_PORT:-12100}"
log="${1:-/tmp/xuzstore_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/xuzstore_c64u_${speed}mhz.status}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
run_id="XUZSTORE-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${XUZSTORE_OUT_DIR:-$readyos_root/logs/xuzstore/$run_id}"
plan="$out_dir/xuzstore-plan.yaml"
fixtures="$out_dir/fixtures"
archive="$out_dir/CREATED.ZIP"
extracted="$out_dir/extracted"

case "$speed" in 1|16|64) ;; *) echo "speed must be 1, 16, or 64 MHz" >&2; exit 64 ;; esac
case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZSTORE-*) ;;
  *) echo "refusing non-owned xuzstore root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$fixtures" "$extracted/NESTED"
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
  echo "Physical C64 Ultimate xuzstore"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
} > "$log"

cd "$readyos_root" || exit 1
run python3 build_support/build_xuzstore_fixture.py "$fixtures" --run-id "$run_id" || exit 1
run env XUZSTORE_RUN_ID="$run_id" XUZSTORE_VOLUME=USB1 \
  /bin/bash probes/xuzstore/build.sh || exit 1
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
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/HELLO.TXT" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/HELLO.TXT" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/BOUNDARY.BIN" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/NESTED/BOUNDARY.BIN" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/ZERO.BIN" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/ZERO.BIN" || exit 2
run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$fixtures/HOSTSTORE.ZIP" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/HOSTSTORE.ZIP" || exit 2
for corrupt_name in TRUNCATED.ZIP MULTIDISK.ZIP TRAVERSAL.ZIP BADCRC.ZIP; do
  run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
    -T "$fixtures/$corrupt_name" \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/SOURCE/$corrupt_name" || exit 2
done
run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$out_dir/owner.readback" || exit 2
cmp -s "$fixtures/owner.marker" "$out_dir/owner.readback" || exit 2
wait_for_url "C64U REST" "http://${host}/v1/drives" || exit 2

sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#remote_root: USB1/READYOS_UZIP_TEST/XUZSTORE-RUN#remote_root: ${remote_root}#g" \
  -e "s#prg: build/xuzstore/xuzstore.prg#prg: ${readyos_root}/build/xuzstore/xuzstore.prg#g" \
  -e "s#stream_port: 12100#stream_port: ${stream_port}#g" \
  -e "s#mhz: 16#mhz: ${speed}#g" \
  build_support/xuzstore_ultimate.generated.yaml > "$plan"

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate plan reported failure; collecting evidence" >> "$log"
fi

cd "$readyos_root" || exit 1
run python3 build_support/analyze_xuzstore_run.py "$log" "$archive" "$fixtures" \
  --probe-only --json-output "$out_dir/probe-result.json" || exit 4
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT/CREATED.ZIP" \
  -o "$archive" || exit 3
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/EXTRACTED/HELLO.TXT" \
  -o "$extracted/HELLO.TXT" || exit 3
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/EXTRACTED/NESTED/BOUNDARY.BIN" \
  -o "$extracted/NESTED/BOUNDARY.BIN" || exit 3
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/EXTRACTED/ZERO.BIN" \
  -o "$extracted/ZERO.BIN" || exit 3
run curl --fail --silent --show-error --max-time 120 --list-only \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/EXTRACTED/" \
  -o "$out_dir/extracted-root.list" || exit 3
mkdir -p "$out_dir/host-extracted/DEEP"
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/HOSTEXTRACTED/HOST.TXT" \
  -o "$out_dir/host-extracted/HOST.TXT" || exit 3
run curl --fail --silent --show-error --max-time 120 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/HOSTEXTRACTED/DEEP/FILE.BIN" \
  -o "$out_dir/host-extracted/DEEP/FILE.BIN" || exit 3
run curl --fail --silent --show-error --max-time 120 --list-only \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/HOSTEXTRACTED/" \
  -o "$out_dir/host-extracted-root.list" || exit 3
run python3 build_support/analyze_xuzstore_run.py "$log" "$archive" "$fixtures" \
  --extracted "$extracted" --extracted-listing "$out_dir/extracted-root.list" \
  --host-extracted "$out_dir/host-extracted" \
  --host-extracted-listing "$out_dir/host-extracted-root.list" \
  --json-output "$out_dir/result.json" || exit 4
echo "XUZSTORE PHYSICAL PASS ${speed}MHz /$remote_root" | tee "$out_dir/PASS.txt" >> "$log"
exit 0
