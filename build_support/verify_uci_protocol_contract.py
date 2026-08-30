#!/usr/bin/env python3
"""Reject known timing-sensitive UCI transaction regressions.

This is intentionally a source-contract check. Physical 1/16 MHz suites remain
the behavioral authority, but this catches the old immediate-IDLE, quiet-delay,
partial-idle-mask, repeated-accept, unbounded recovery, and
unacknowledged-overflow patterns before a build.
"""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
ERRORS: list[str] = []


def text(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8")


def require(rel: str, needles: tuple[str, ...]) -> None:
    source = text(rel)
    for needle in needles:
        if needle not in source:
            ERRORS.append(f"{rel}: missing required UCI contract marker {needle!r}")


def forbid(rel: str, patterns: tuple[str, ...]) -> None:
    source = text(rel)
    for pattern in patterns:
        if re.search(pattern, source, re.MULTILINE):
            ERRORS.append(f"{rel}: forbidden legacy UCI pattern /{pattern}/")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.DOTALL)
    if not match:
        return ""
    depth = 1
    pos = match.end()
    while pos < len(source) and depth:
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
        pos += 1
    return source[match.end(): pos - 1]


def label_block(source: str, start: str, end: str) -> str:
    match = re.search(
        rf"(?ms)^{re.escape(start)}:\s*$\n(.*?)(?=^{re.escape(end)}:\s*$)",
        source,
    )
    return match.group(1) if match else ""


c_transports = {
    "src/apps/readyirc/readyirc_uci.c": ("wait_idle", "wait_response_state"),
    "src/apps/sysinfo/sysinfo_uci.c": ("uci_wait_idle", "uci_wait_response_state"),
    "src/apps/ucitest/ucitest_uci.c": ("wait_idle", "wait_response_state"),
    "src/apps/uzip/uz_uci.c": ("wait_idle", "wait_response_state"),
}
for rel, (idle_name, response_name) in c_transports.items():
    require(
        rel,
        (
            "UCI_STAT_ACCEPT",
            "UCI_STAT_ERROR",
            "quiet_idle",
            response_name,
            "PUSH_CMD is asynchronous",
            "DATA_ACC is asynchronous",
            "wait_accept_advance",
            "abort_and_recover",
            "UCI_WAIT_PASSES",
            "drain_tries",
            "asm_accept_data();",
        ),
    )
    forbid(
        rel,
        (
            r"\bdrain_guard\b",
            r"\bUCI_WAIT_SHORT\b",
            r"if\s*\(state\s*==\s*UCI_STATE_IDLE\)",
            r"state\s*==\s*UCI_STATE_LAST\s*\|\|\s*state\s*==\s*UCI_STATE_MORE\s*\|\|\s*state\s*==\s*UCI_STATE_IDLE",
        ),
    )
    source = text(rel)
    idle = function_body(source, idle_name)
    response = function_body(source, response_name)
    if not idle:
        ERRORS.append(f"{rel}: could not locate {idle_name} body")
    elif "clear_error" in idle:
        ERRORS.append(f"{rel}: {idle_name} must report ERROR, not clear it")
    if not response:
        ERRORS.append(f"{rel}: could not locate {response_name} body")
    elif "UCI_STATE_IDLE" in response:
        ERRORS.append(f"{rel}: {response_name} must accept only LAST/MORE")
    sync_name = "uci_sync_interface" if "sysinfo" in rel else "sync_interface"
    sync = function_body(source, sync_name)
    if "asm_abort();" in sync:
        ERRORS.append(f"{rel}: {sync_name} must service ABORT_P, not re-issue ABORT")
    if re.search(r"(?m)^\s*tries\s*=\s*0u\s*;", sync):
        ERRORS.append(f"{rel}: {sync_name} resets its failure bound while servicing stuck flags")
    if "asm_accept_data();" in sync and "wait_accept_advance(state)" not in sync:
        ERRORS.append(f"{rel}: recovery can re-issue an asynchronous DATA_ACC")
    if "UCI_WAIT_PASSES" not in sync:
        ERRORS.append(f"{rel}: recovery wait must retain a bounded top-speed pass count")
    accept_name = "uci_wait_accept_advance" if "sysinfo" in rel else "wait_accept_advance"
    for wait_name in (idle_name, response_name, accept_name):
        wait_body = function_body(source, wait_name)
        if "UCI_WAIT_PASSES" not in wait_body:
            ERRORS.append(f"{rel}: {wait_name} must retain a bounded top-speed pass count")
    recover = function_body(source, "uci_abort_and_recover" if "sysinfo" in rel else "abort_and_recover")
    if "UCI_STAT_ABORT" not in recover or "== 0u" not in recover:
        ERRORS.append(f"{rel}: abort recovery must check ABORT_P before requesting ABORT")

# SETUP deliberately carries the later ReadyFS form of the ReadyIRC transport:
# identical state-machine contract, plus an assembly-adjacent MORE->BUSY edge
# observer proven at 64 MHz. Keep this separate from the older symbol spellings
# above so a mechanical rename is never mistaken for protocol evidence.
setup_transport = "src/setup/setup_uci.c"
require(
    setup_transport,
    (
        "quiet_idle",
        "wait_response",
        "PUSH is asynchronous",
        "wait_advance",
        "sync_interface",
        "recover",
        "UCI_PASSES",
        "setup_uci_asm_accept_more_transition",
        "drain",
    ),
)
setup_source = text(setup_transport)
for wait_name in ("wait_idle", "wait_response", "wait_advance"):
    body = function_body(setup_source, wait_name)
    if not body or "UCI_PASSES" not in body:
        ERRORS.append(f"{setup_transport}: {wait_name} must retain bounded top-speed passes")
setup_sync = function_body(setup_source, "sync_interface")
if "setup_uci_asm_abort" in setup_sync:
    ERRORS.append(f"{setup_transport}: sync_interface must not re-issue ABORT_P")
setup_recover = function_body(setup_source, "recover")
if "UCI_ABORT" not in setup_recover or "== 0u" not in setup_recover:
    ERRORS.append(f"{setup_transport}: recovery must check ABORT_P before requesting ABORT")
setup_command = function_body(setup_source, "setup_uci_command")
if "setup_uci_asm_accept_more_transition(state)" not in setup_command:
    ERRORS.append(f"{setup_transport}: MORE blocks must use adjacent accept/transition helper")

require(
    "src/setup/setup_uci_asm.s",
    (
        "setup_uci.c owns synchronization",
        "_setup_uci_asm_accept_more_transition",
        "The bound detects failure; it is not pacing",
        "more_write",
        "more_read",
    ),
)


asm_transports = (
    "src/apps/launcher/launcher_uci_dma.s",
    "probes/uci_dma/uci_dma_probe.s",
)
for rel in asm_transports:
    require(
        rel,
        (
            "UCI_STAT_ACCEPT",
            "UCI_QUIET_MASK = $3F",
            "PUSH_CMD is asynchronous",
            "DATA_ACC is asynchronous",
            "wait_accept_advance",
            "only LAST/MORE",
            "abort_and_recover",
            "UCI_STATE_WAIT_PASSES = $08",
            "4096",
            "jsr uci_accept_data",
            "jsr wait_idle",
        ),
    )
    source = text(rel)
    wait_response = label_block(source, "wait_data_state", "wait_idle")
    if not wait_response:
        ERRORS.append(f"{rel}: could not locate wait_data_state block")
    elif re.search(r"cmp\s+#UCI_STATE_IDLE", wait_response):
        ERRORS.append(f"{rel}: wait_data_state still accepts immediate IDLE")
    wait_idle = label_block(source, "wait_idle", "drain_response")
    if "#UCI_STAT_ERROR" not in wait_idle or "#UCI_QUIET_MASK" not in wait_idle:
        ERRORS.append(f"{rel}: wait_idle must reject ERROR and require quiet mask")
    for label in ("drain_wait_byte", "drain_load_wait_byte"):
        if f"{label}:" not in source:
            continue
        block = re.search(rf"(?ms)^{label}:\s*$\n(.*?)(?=^[A-Za-z_][A-Za-z0-9_]*:\s*$)", source)
        if block and ("dey" in block.group(1) or "dec timeout" in block.group(1)):
            ERRORS.append(f"{rel}: {label} uses a quiet delay as response pacing")
    sync = label_block(source, "sync_interface", "abort_and_recover")
    if "jsr uci_abort" in sync:
        ERRORS.append(f"{rel}: sync_interface re-issues an already pending ABORT")
    if "timeout_outer" not in sync or "dec timeout_outer" not in sync:
        ERRORS.append(f"{rel}: sync_interface needs a bounded multi-pass failure limit")
    if "jsr uci_accept_data" in sync and "jsr wait_accept_advance" not in sync:
        ERRORS.append(f"{rel}: recovery can re-issue an asynchronous DATA_ACC")
    recover = label_block(source, "abort_and_recover", "wait_data_state")
    if "jsr uci_status" not in recover or "#UCI_STAT_ABORT" not in recover:
        ERRORS.append(f"{rel}: abort recovery must sample current ABORT_P before requesting ABORT")


require(
    "src/apps/readyirc/readyirc.c",
    ("fully acknowledged UCI transaction", "One state-driven UCI poll"),
)
require(
    "src/apps/sysinfo/sysinfo.c",
    ("Sole app-level UCI command gateway", "Every command below goes"),
)
require(
    "src/apps/ucitest/ucitest.c",
    ("never imply command completion", "One complete state-driven transaction"),
)
require(
    "src/apps/launcher/launcher.c",
    ("Assembly owns the complete asynchronous UCI lifecycle", "One assembly-owned UCI sequence"),
)
require(
    "src/setup/setup_backend.c",
    (
        "sole state-machine gateway",
        "multi-block stream",
        "MOUNT follows the same target-status contract",
    ),
)
require(
    "src/apps/uzip/uz_uci.h",
    (
        "sole UCI transaction gateway",
        "asynchronous PUSH/ABORT handling",
        "complete data/status queue draining",
        "final quiet-IDLE wait",
    ),
)
require(
    "src/apps/uzip/uz_uci_asm.s",
    (
        "uz_uci.c owns synchronization",
        "_uz_uci_asm_accept_more_transition",
        "The bound detects failure; it is not pacing",
        "more_write",
        "more_read",
    ),
)
require(
    "src/apps/uzip/uz_dos.c",
    (
        "Every Ultimate DOS operation uses the shared uZIP state-machine",
        "asynchronous PUSH/ABORT",
        "complete queue drains",
        "final quiet-IDLE wait",
        "READ_DIR uses the same complete async gateway",
    ),
)
require(
    "build_support/run_xuzio_c64u.sh",
    (
        "run-ultimate-plan",
        ".READYOS-UZIP-OWNER",
        "refusing non-owned xuzio root",
    ),
)
require(
    "probes/uci_timing/uci_timing_probe.c",
    ("Probe the production System Info transport itself",),
)
require(
    "probes/uci_dma/uci_dma_probe.s",
    (
        "uci_dma_image_name.inc",
        "exactly matches",
        "Ultimate DOS memory-read payload length: 64 bytes",
    ),
)
probe_source = text("probes/uci_dma/uci_dma_probe.s")
probe_read_64 = label_block(probe_source, "dos_read_ram_sync_ok", "dos_seek_payload")
if not re.search(r"lda\s+#\$40\s*\n\s*jsr\s+uci_write_cmd", probe_read_64):
    ERRORS.append(
        "probes/uci_dma/uci_dma_probe.s: dos_read_ram_64 must request exactly $0040 bytes"
    )
require(
    "build_support/run_uci_dma_probe_ultimate.sh",
    ("run_tag=", "UCI_DMA_IMAGE_NAME", "candidate_image=\"${candidate}/${image_name}\""),
)
require(
    "build_support/run_uci_timing_probe_ultimate.sh",
    ("run_tag=", "UCI_TIMING_IMAGE_NAME", "image_name=\"${remote_image##*/}\""),
)
require(
    "build_support/uci_timing_probe_ultimate.generated.yaml",
    (
        "wait_probe_loaded",
        "set_probe_speed",
        "start_probe_at_test_speed",
        "restore_probe_speed",
    ),
)


if ERRORS:
    print("UCI PROTOCOL CONTRACT VERIFICATION FAILED", file=sys.stderr)
    for error in ERRORS:
        print(f"- {error}", file=sys.stderr)
    raise SystemExit(1)

print("UCI PROTOCOL CONTRACT VERIFICATION PASSED: 5 C transports, 4 asm/accessor transports, 9 call-site groups")
