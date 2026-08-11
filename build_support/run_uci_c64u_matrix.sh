#!/usr/bin/env bash
set -euo pipefail

# Comprehensive UCI regression matrix. Launch only from a Terminal-owned or
# long-running background shell because every child talks to the C64 Ultimate.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READYOS_ROOT="${READYOS_ROOT:-$(cd "$SCRIPT_DIR/.." && pwd)}"
C64U_HOST="${C64U_HOST:-10.0.0.79}"
OUT_DIR="${UCI_C64U_MATRIX_OUT_DIR:-$READYOS_ROOT/logs/uci_c64u_matrix}"
SPEEDS="${UCI_C64U_MATRIX_SPEEDS:-1 16}"

mkdir -p "$OUT_DIR"
cd "$READYOS_ROOT"
python3 build_support/verify_uci_protocol_contract.py

if [ -n "${UCI_C64U_MATRIX_D81:-}" ]; then
  D81="$UCI_C64U_MATRIX_D81"
else
  PUBLIC_VERSION="$(python3 build_support/update_build_version.py --current)"
  PUBLIC_VERSION="${PUBLIC_VERSION%[A-Z]}"
  D81="$(ls -t "Releases/$PUBLIC_VERSION/precog-d81/"*.d81 | head -1)"
fi

cleanup_hardware() {
  set +e
  C64U_HOST="$C64U_HOST" C64U_SKIP_UPLOAD=1 C64U_SKIP_CONFIG=1 \
  READYOS_CLEAR_REU=1 READYOS_CLEAR_REU_ONLY=1 \
    /bin/bash build_support/run_readyos_boot_c64u_rest.sh \
      "$D81" uci-matrix-clear.d81 "$OUT_DIR/final-reu-clear" \
      >>"$OUT_DIR/final-cleanup.log" 2>&1
  /usr/bin/curl --fail --silent --show-error --request PUT \
    "http://$C64U_HOST/v1/machine:reboot" \
    --output "$OUT_DIR/final-reboot.json" \
    >>"$OUT_DIR/final-cleanup.log" 2>&1
  sleep 4
  /usr/bin/curl --fail --silent --show-error --request PUT \
    "http://$C64U_HOST/v1/machine:resume" \
    --output "$OUT_DIR/final-resume.json" \
    >>"$OUT_DIR/final-cleanup.log" 2>&1
}
trap cleanup_hardware EXIT

run_case() {
  local name="$1"
  shift
  echo "START $name"
  "$@" >"$OUT_DIR/$name.log" 2>&1
  echo "PASS $name"
}

for speed in $SPEEDS; do
  case "$speed" in
    1|2|3|4|6|8|10|12|14|16|20|24|32|40|48|64) ;;
    *) echo "Unsupported matrix speed: $speed" >&2; exit 64 ;;
  esac

  run_case "dma-probe-${speed}mhz" \
    env C64U_HOST="$C64U_HOST" PROBE_IMAGE_TYPE=d81 \
      UCI_DMA_SPEED_MHZ="$speed" \
      /bin/bash build_support/run_uci_dma_probe_ultimate.sh \
      "$OUT_DIR/dma-probe-${speed}mhz.runner.log" \
      "$OUT_DIR/dma-probe-${speed}mhz.status"

  run_case "timing-probe-${speed}mhz" \
    env C64U_HOST="$C64U_HOST" UCI_TIMING_SPEED_MHZ="$speed" \
      BASE_D81="$D81" \
      /bin/bash build_support/run_uci_timing_probe_ultimate.sh \
      "$OUT_DIR/timing-probe-${speed}mhz.runner.log" \
      "$OUT_DIR/timing-probe-${speed}mhz.status"

  run_case "sysinfo-${speed}mhz" \
    env C64U_HOST="$C64U_HOST" SYSINFO_C64U_SPEED_MHZ="$speed" \
      SYSINFO_C64U_SKIP_BUILD=1 SYSINFO_C64U_D81="$D81" \
      SYSINFO_C64U_OUT_DIR="$OUT_DIR/sysinfo-${speed}mhz" \
      /bin/bash build_support/run_sysinfo_c64u_smoke.sh

  run_case "ucitest-${speed}mhz" \
    env C64U_HOST="$C64U_HOST" UCITEST_C64U_SPEED_MHZ="$speed" \
      UCITEST_C64U_SKIP_BUILD=1 UCITEST_C64U_D81="$D81" \
      UCITEST_C64U_OUT_DIR="$OUT_DIR/ucitest-${speed}mhz" \
      /bin/bash build_support/run_ucitest_c64u_smoke.sh

  run_case "readyirc-${speed}mhz" \
    env C64U_HOST="$C64U_HOST" READYIRC_C64U_SPEED_MHZ="$speed" \
      READYIRC_C64U_SKIP_BUILD=1 READYIRC_C64U_D81="$D81" \
      READYIRC_C64U_OUT_DIR="$OUT_DIR/readyirc-${speed}mhz" \
      /bin/bash build_support/run_readyirc_c64u_suite.sh
done

echo "UCI C64 Ultimate matrix passed at speeds: $SPEEDS"
echo "Artifacts: $OUT_DIR"
