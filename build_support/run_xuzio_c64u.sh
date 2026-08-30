#!/usr/bin/env bash
set -uo pipefail

readyos_root="${READYOS_ROOT:-/Users/karlprosserpp/dev/c64projects/readyosprecog}"
harness="${ULTIMATE_HARNESS_ROOT:-$readyos_root/../agenticdevharness/tools/vice_tasks_dotnet}"
host="${C64U_HOST:-10.0.0.79}"
speed="${XUZIO_SPEED_MHZ:-16}"
stream_port="${XUZIO_STREAM_PORT:-12000}"
log="${1:-/tmp/xuzio_c64u_${speed}mhz.log}"
status_file="${2:-/tmp/xuzio_c64u_${speed}mhz.status}"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"
run_id="XUZIO-$(date +%Y%m%d-%H%M%S)-${speed}MHZ-$$"
remote_parent="USB1/READYOS_UZIP_TEST"
remote_root="$remote_parent/$run_id"
out_dir="${XUZIO_OUT_DIR:-$readyos_root/logs/xuzio/$run_id}"
plan="$out_dir/xuzio-plan.yaml"
downloads="$out_dir/downloads"
owner_file="$out_dir/owner.marker"

case "$speed" in
  1|16|64) ;;
  *) echo "xuzio acceptance speed must be 1, 16, or 64 MHz" >&2; exit 64 ;;
esac
case "$remote_root" in
  USB1/READYOS_UZIP_TEST/XUZIO-*) ;;
  *) echo "refusing non-owned xuzio root: $remote_root" >&2; exit 64 ;;
esac

mkdir -p "$out_dir" "$downloads"
rm -f "$log" "$status_file"

finish() {
  rc=$?
  echo "$rc" > "$status_file"
  echo "EXIT $rc" >> "$log"
  exit "$rc"
}
trap finish EXIT

run() {
  echo "+ $*" >> "$log"
  "$@" >> "$log" 2>&1
}

wait_for_url() {
  label="$1"
  url="$2"
  deadline=$((SECONDS + connect_wait_s))
  echo "Waiting for $label: $url" >> "$log"
  while [ "$SECONDS" -lt "$deadline" ]; do
    if curl --fail --silent --show-error --max-time 5 "$url" >/dev/null 2>>"$log"; then
      echo "$label reachable" >> "$log"
      return 0
    fi
    sleep 3
  done
  echo "$label not reachable after ${connect_wait_s}s" >> "$log"
  return 1
}

{
  date
  echo "Physical C64 Ultimate xuzio"
  echo "Host: $host"
  echo "CPU speed: ${speed} MHz"
  echo "Owned remote root: /$remote_root"
  echo "Preserve root: yes"
} > "$log"

cd "$readyos_root" || exit 1
if ! run env XUZIO_RUN_ID="$run_id" XUZIO_VOLUME=USB1 \
  /bin/bash probes/xuzio/build.sh; then
  exit 1
fi

python3 - "$owner_file" "$run_id" <<'PY'
import pathlib, sys
pathlib.Path(sys.argv[1]).write_bytes(sys.argv[2].encode("ascii"))
PY

if ! wait_for_url "C64U FTP" "ftp://anonymous:anonymous%40@${host}/USB1/"; then
  exit 2
fi
existing="$out_dir/existing-roots.txt"
if curl --fail --silent --show-error --max-time 30 --list-only \
  "ftp://anonymous:anonymous%40@${host}/${remote_parent}/" > "$existing" 2>>"$log"; then
  if grep -Fqx "$run_id" "$existing"; then
    echo "fresh owned root already exists; refusing mutation: /$remote_root" >> "$log"
    exit 2
  fi
fi

if ! run curl --fail --silent --show-error --ftp-create-dirs --max-time 90 \
  -T "$owner_file" \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER"; then
  exit 2
fi
marker_readback="$out_dir/owner.readback"
if ! run curl --fail --silent --show-error --max-time 30 \
  "ftp://anonymous:anonymous%40@${host}/${remote_root}/.READYOS-UZIP-OWNER" \
  -o "$marker_readback"; then
  exit 2
fi
if ! cmp -s "$owner_file" "$marker_readback"; then
  echo "ownership marker readback mismatch" >> "$log"
  exit 2
fi
if ! wait_for_url "C64U REST" "http://${host}/v1/drives"; then
  exit 2
fi

sed \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  -e "s#remote_root: USB1/READYOS_UZIP_TEST/XUZIO-RUN#remote_root: ${remote_root}#g" \
  -e "s#prg: build/xuzio/xuzio.prg#prg: ${readyos_root}/build/xuzio/xuzio.prg#g" \
  -e "s#stream_port: 12000#stream_port: ${stream_port}#g" \
  -e "s#mhz: 16#mhz: ${speed}#g" \
  "$readyos_root/build_support/xuzio_ultimate.generated.yaml" > "$plan"

cd "$harness" || exit 1
if ! ULTIMATE_ASSUME_MOUNTED=1 /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan --plan "$plan" --no-tui >> "$log" 2>&1; then
  echo "physical Ultimate plan reported failure; collecting available evidence" >> "$log"
fi

cd "$readyos_root" || exit 1
if ! run python3 "$readyos_root/build_support/analyze_xuzio_run.py" \
  "$log" "$downloads" --probe-only \
  --json-output "$out_dir/probe-result.json"; then
  echo "physical C64 probe result failed; preserving owned root and evidence" >> "$log"
  exit 4
fi

for name in S00000.BIN S00001.BIN S00511.BIN S00512.BIN S00513.BIN \
            S04095.BIN S04096.BIN S65535.BIN S65536.BIN RENAMED.OK; do
  if ! run curl --fail --silent --show-error --max-time 120 \
    "ftp://anonymous:anonymous%40@${host}/${remote_root}/OUTPUT_MIX/${name}" \
    -o "$downloads/$name"; then
    exit 3
  fi
done

if ! run python3 "$readyos_root/build_support/analyze_xuzio_run.py" \
  "$log" "$downloads" --json-output "$out_dir/result.json"; then
  exit 4
fi

echo "XUZIO PHYSICAL PASS ${speed}MHz /$remote_root" | tee "$out_dir/PASS.txt" >> "$log"
exit 0
