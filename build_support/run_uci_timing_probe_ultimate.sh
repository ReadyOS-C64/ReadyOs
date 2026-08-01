#!/usr/bin/env bash
set -uo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
harness="/Users/karlprosserpp/dev/c64projects/agenticdevharness/tools/vice_tasks_dotnet"
log="${1:-/tmp/uci_timing_probe_ultimate.log}"
status="${2:-/tmp/uci_timing_probe_ultimate.status}"
host="${C64U_HOST:-10.0.0.79}"
remote_image="${C64U_REMOTE_IMAGE:-USB1/readyos.d81}"
plan="/tmp/uci_timing_probe_ultimate.yaml"
connect_wait_s="${C64U_CONNECT_WAIT_S:-300}"

rm -f "$log" "$status"

finish() {
  rc=$?
  echo "$rc" > "$status"
  echo "EXIT $rc" >> "$log"
  exit "$rc"
}
trap finish EXIT

run() {
  echo "+ $*" >> "$log"
  "$@" >> "$log" 2>&1
}

wait_for_url() {
  local label="$1"
  local url="$2"
  local deadline=$((SECONDS + connect_wait_s))
  echo "Waiting for ${label}: ${url}" >> "$log"
  while [ "$SECONDS" -lt "$deadline" ]; do
    if curl --fail --silent --show-error --max-time 5 "$url" >/dev/null 2>>"$log"; then
      echo "${label} reachable" >> "$log"
      return 0
    fi
    sleep 3
  done
  echo "${label} not reachable after ${connect_wait_s}s" >> "$log"
  return 1
}

{
  date
  echo "C64U host: $host"
  echo "Remote image: /$remote_image"
} > "$log"

cd "$repo" || exit 1
if ! run /bin/bash probes/uci_timing/build.sh; then
  exit 1
fi

remote_root="${remote_image%%/*}"
sed \
  -e "s#remote_root: USB1#remote_root: ${remote_root}#g" \
  -e "s#remote_disk: USB1/readyos.d81#remote_disk: ${remote_image}#g" \
  -e "s#host: 10.0.0.79#host: ${host}#g" \
  "$repo/build_support/uci_timing_probe_ultimate.generated.yaml" > "$plan"

echo "Replacing /$remote_image" >> "$log"
if ! wait_for_url "C64U FTP" "ftp://anonymous:anonymous%40@${host}/${remote_root}/"; then
  exit 2
fi
run curl --max-time 20 --quote "DELE /${remote_image}" \
  "ftp://anonymous:anonymous%40@${host}/" || true
if ! run curl --ftp-create-dirs --max-time 90 \
  -T "$repo/build/uci_timing/readyos.d81" \
  "ftp://anonymous:anonymous%40@${host}/${remote_image}"; then
  exit 2
fi
if ! wait_for_url "C64U REST" "http://${host}/v1/drives"; then
  exit 2
fi

cd "$harness" || exit 1
if ! /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan \
  --plan "$plan" \
  --no-tui >> "$log" 2>&1; then
  echo "Ultimate plan failed" >> "$log"
  exit 3
fi

if ! run python3 "$repo/build_support/analyze_uci_timing_probe_run.py" \
  "$log" --json-output "$repo/build/uci_timing/latest_result.json"; then
  echo "Timing probe reported failure" >> "$log"
  exit 4
fi

exit 0
