#!/usr/bin/env bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd)"
host="${C64U_HOST:-10.0.0.79}"
out_dir="${1:-/tmp/uci_dma_probe_manual_capture_$(date +%Y%m%d_%H%M%S)}"

mkdir -p "$out_dir"

curl_get() {
  local url="$1"
  local output="$2"
  curl --fail --silent --show-error --max-time 10 "$url" -o "$output"
}

curl_get "http://${host}/v1/machine:readmem?address=0400&length=1000" "$out_dir/screen_0400.bin"
curl_get "http://${host}/v1/machine:readmem?address=3000&length=128" "$out_dir/probe_results_3000.bin"
curl_get "http://${host}/v1/machine:readmem?address=4000&length=256" "$out_dir/dma_load_buf_4000.bin"
curl_get "http://${host}/v1/machine:readmem?address=5000&length=256" "$out_dir/reu_fetch_buf_5000.bin"
curl_get "http://${host}/v1/machine:readmem?address=6000&length=256" "$out_dir/reu_snap_udma1_6000.bin"
curl_get "http://${host}/v1/machine:readmem?address=6100&length=256" "$out_dir/reu_snap_udma2_6100.bin"
curl_get "http://${host}/v1/machine:readmem?address=6200&length=256" "$out_dir/reu_snap_udma3_6200.bin"
curl_get "http://${host}/v1/machine:readmem?address=6800&length=2048" "$out_dir/launcher_code_6800.bin"
curl_get "http://${host}/v1/machine:readmem?address=B800&length=1536" "$out_dir/launcher_bss_b800.bin"
curl_get "http://${host}/v1/drives" "$out_dir/drives.json"

python3 "$repo/build_support/decode_c64_screen.py" \
  "$out_dir/screen_0400.bin" \
  --output "$out_dir/screen.txt"

for bin in "$out_dir"/*.bin; do
  xxd -g 1 -c 16 "$bin" > "${bin%.bin}.hex.txt"
done

echo "$out_dir"
cat "$out_dir/screen.txt"
