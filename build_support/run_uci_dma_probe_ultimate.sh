#!/usr/bin/env bash
set -uo pipefail

repo="/Users/karlprosserpp/dev/c64projects/readyosprecog"
harness="/Users/karlprosserpp/dev/c64projects/agenticdevharness/tools/vice_tasks_dotnet"
log="${1:-/tmp/uci_dma_probe_ultimate.log}"
status="${2:-/tmp/uci_dma_probe_ultimate.status}"

host="${C64U_HOST:-10.0.0.79}"
image_type="${PROBE_IMAGE_TYPE:-d81}"
speed_mhz="${UCI_DMA_SPEED_MHZ:-16}"
run_tag="$(date +%Y%m%d-%H%M%S)-${speed_mhz}mhz-$$"
case "$speed_mhz" in
  1|2|3|4|6|8|10|12|14|16|20|24|32|40|48|64) ;;
  *) echo "UCI_DMA_SPEED_MHZ is not supported: $speed_mhz" >&2; exit 64 ;;
esac
case "$image_type" in
  d64)
    image_upper="D64"
    image_name="UCI40-${run_tag}.D64"
    drive_type="1541"
    expected_title="UCI DOS REU PROBE V40"
    expected_version="40"
    ;;
  d81)
    image_upper="D81"
    image_name="UCI41-${run_tag}.D81"
    drive_type="1581"
    expected_title="UCI DOS REU PROBE V41"
    expected_version="41"
    ;;
  *)
    echo "PROBE_IMAGE_TYPE must be d64 or d81" > "$log"
    echo 64 > "$status"
    exit 64
    ;;
esac
requested_host="$host"
if [[ -n "${C64U_REMOTE_DIR:-}" ]]; then
  case "$C64U_REMOTE_DIR" in
    USB0|USB1) candidate_dirs=("$C64U_REMOTE_DIR") ;;
    *)
      echo "C64U_REMOTE_DIR must be USB0 or USB1" > "$log"
      echo 64 > "$status"
      exit 64
      ;;
  esac
else
  candidate_dirs=("USB1" "USB0")
fi
remote_dir=""
remote_image=""
plan=""
rc=99

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

probe_endpoint() {
  echo "+ $*" >> "$log"
  "$@" >> "$log" 2>&1
  local probe_rc=$?
  echo "endpoint rc=${probe_rc}" >> "$log"
  return 0
}

{
  date
  echo "Requested C64U host: ${requested_host}"
  echo "Discovery mode: ${C64U_DISCOVER:-0}"
  echo "CPU speed: ${speed_mhz} MHz"
} > "$log"

if [[ "$requested_host" == "auto" || "${C64U_DISCOVER:-0}" == "1" ]]; then
  discovery_log="${C64U_DISCOVERY_LOG:-${log}.discover}"
  echo "Running C64U discovery, log: ${discovery_log}" >> "$log"
  if discovered_host="$(
    C64U_DISCOVERY_LOG="$discovery_log" \
    C64U_HOST="$requested_host" \
    C64U_PASSWORD="${C64U_PASSWORD:-}" \
    C64U_HOSTS="${C64U_HOSTS:-}" \
    C64U_SCAN_SUBNET="${C64U_SCAN_SUBNET:-}" \
    /bin/bash "$repo/build_support/discover_c64u_host.sh" 2>>"$log"
  )"; then
    host="$discovered_host"
    echo "Discovered C64U host: ${host}" >> "$log"
  else
    echo "C64U discovery found no reachable Ultimate REST host" >> "$log"
    if [[ "$requested_host" == "auto" ]]; then
      exit 6
    fi
    echo "Falling back to requested host: ${host}" >> "$log"
  fi
fi

{
  echo "C64U host: ${host}"
  echo "Remote candidates: ${candidate_dirs[*]}"
  echo "REST preflight"
} >> "$log"

probe_endpoint curl --max-time 5 --silent --show-error --head "http://${host}/"
echo "FTP preflight" >> "$log"
probe_endpoint curl --max-time 5 --silent --show-error \
  "ftp://anonymous:anonymous%40@${host}/"

echo "Building Ultimate DOS REU probe ${image_upper} and payloads" >> "$log"

cd "$repo" || exit 1
if ! run env PROBE_IMAGE_TYPE="$image_type" UCI_DMA_IMAGE_NAME="$image_name" \
  /bin/bash probes/uci_dma/build.sh; then
  exit 1
fi

for candidate in "${candidate_dirs[@]}"; do
  candidate_image="${candidate}/${image_name}"
  candidate_url="ftp://anonymous:anonymous%40@${host}/${candidate_image}"
  candidate_plan="/tmp/uci_dma_probe_ultimate_${candidate}.yaml"

  sed \
    -e "s#remote_root: USB1#remote_root: ${candidate}#g" \
    -e "s#remote_disk: USB1/UCI.D81#remote_disk: ${candidate_image}#g" \
    -e "s#uci_dma_probe.d81#uci_dma_probe.${image_type}#g" \
    -e "s#drive_a_type: '1581'#drive_a_type: '${drive_type}'#g" \
    -e "s#drive_b_type: '1581'#drive_b_type: '${drive_type}'#g" \
    -e "s#drive_type: '1581'#drive_type: '${drive_type}'#g" \
    -e "s#UCI DOS REU PROBE V41#${expected_title}#g" \
    -e "s#mhz: 16#mhz: ${speed_mhz}#g" \
    "$repo/build_support/uci_dma_probe_ultimate.generated.yaml" > "$candidate_plan"

  echo "Replacing /${candidate_image}" >> "$log"
  run curl --max-time 20 --quote "DELE /${candidate_image}" \
    "ftp://anonymous:anonymous%40@${host}/" || true

  if run curl --ftp-create-dirs --max-time 60 \
    -T "$repo/build/uci_dma_probe/uci_dma_probe.${image_type}" \
    "$candidate_url"; then
    remote_dir="$candidate"
    remote_image="$candidate_image"
    plan="$candidate_plan"
    break
  fi

  echo "FTP upload failed for /${candidate_image}" >> "$log"
done

if [[ -z "$remote_dir" ]]; then
  echo "FTP upload failed for all candidate roots" >> "$log"
  exit 2
fi

echo "Using remote image: /${remote_image}" >> "$log"

cd "$harness" || exit 1
if ! /usr/local/share/dotnet/dotnet run \
  --project src/ViceTasks.Binary/ViceTasks.Binary.csproj \
  -- run-ultimate-plan \
  --plan "$plan" \
  --no-tui >> "$log" 2>&1; then
  echo "Ultimate plan failed" >> "$log"
  exit 3
fi

manifest="$(
  python3 - "$log" <<'PY'
import re
import sys

text = open(sys.argv[1], "r", encoding="utf-8-sig", errors="ignore").read()
matches = re.findall(r'"Manifest"\s*:\s*"([^"]+)"', text)
print(matches[-1] if matches else "")
PY
)"
if [[ -z "$manifest" ]]; then
  echo "Ultimate plan manifest not found in log" >> "$log"
  exit 4
fi

run_dir="$(dirname "$manifest")"
if ! run python3 "$repo/build_support/analyze_uci_dma_probe_run.py" \
  "$run_dir" --expect success --version "$expected_version"; then
  echo "Ultimate probe artifact analysis failed" >> "$log"
  exit 5
fi

exit 0
