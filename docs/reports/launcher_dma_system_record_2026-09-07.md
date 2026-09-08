# Launcher DMA system-record verification — 2026-09-07

DMA configuration, validation status, successful-use history, and last error
now live in the public `DM` v1 record at ReadyOS-bank offset `$FCC0`. The
[system architecture](../ReadyOS_SHIM_ARCHITECTURE_0.5.md#dma-service-record)
defines its byte layout, header descriptor, ownership, and lifecycle.

## Shared UI framework acceptance

The canonical regression now runs inside `vice_tasks_dotnet`. The generator
only emits test fixtures and plans; it does not implement a device transport.
The framework owns launch, UI input, screen/state capture, assertions, CPU
speed control, cleanup, `events.ndjson`, and the final `manifest.json`.

VICE input uses the framework's supported `monitor.command` fallback to write
each keyboard byte/count atomically while paused. All other VICE operations
remain framework-managed binary-monitor operations. Ultimate uses the normal
framework `input.sequence` and `ultimate.speed.set` steps.

Framework results are recorded below after each final manifest is checked.

- Regular D81 0.5Z: **success**, zero degraded steps, 52 steps.
  `logs/vice_auto_20260907_222441/manifest.json`.
- EasyFlash 0.5X: **success**, zero degraded steps, 46 steps.
  `logs/vice_auto_20260907_222515/manifest.json`.
- Physical Ultimate 0.5V at 1/16/64 MHz: **success**, zero degraded steps,
  116 steps. `logs/ultimate_auto_20260907_223844/manifest.json`.
  Framework cleanup restored 1 MHz. All nine menu-return state captures
  retain the expected screen character at `$052C`, not the diagnostic `5`.

The plans preserve an Editor text marker across three Ctrl-B returns and a
round trip through ReadyShell, execute `VER` in the shell, and finish at the
menu. Ultimate repeats the checks at 1, 16, and 64 MHz; VICE covers regular D81
and EasyFlash with its companion D64. Every successful UI checkpoint includes
framework-managed screenshots and state artifacts.

## Earlier diagnostics and failed attempts

Standalone drivers were removed in favor of the shared framework. Their
results are supplemental only, not the primary acceptance above:

- `logs/launcher-return-20260907/ultimate-system-record/`: at 1/16/64 MHz,
  three disk-ejected returns preserved Editor text and `DMA:ON`, left the
  image ejected, and left no diagnostic `5`. A subsequent cold ReadyShell
  load remounted the configured image. All six cold loads reported `OK`.
- `logs/launcher-return-20260907/d81-text-monitor/` and
  `logs/launcher-return-20260907/easyflash-text-monitor/`: earlier standalone
  VICE checks. These are superseded by the framework manifests above.

An ejected image staying ejected demonstrates that warm return did not run
the old validation/mount path; `AVAILABLE` does not mean currently mounted.

Standard binary-key-input VICE runs timed out on resume, including a control
build with the previous launcher implementation. These are failures, not
passes. The shared framework's supported atomic-input fallback resolves the
test-input issue without replacing the framework.

The first Ultimate framework run
(`logs/ultimate_auto_20260907_222349/manifest.json`) timed out during boot input. A subsequent
run (`logs/ultimate_auto_20260907_223017/manifest.json`) passed return/resume
checks at 1/16 MHz but failed an incorrect expectation for ReadyShell's
cold-only help hint on a warm resume. The corrected plan checks the persistent
shell header instead. Neither failed manifest counts as acceptance.

## Reproduction

Build through the normal release workflow:

```sh
/bin/bash ./run.sh --profile precog-ultimate --build-only
/bin/bash ./run.sh --profile precog-d81 --build-only
/bin/bash ./run.sh easyflash --build-only
```

Generate a plan using the built disk plus the relevant PREBOOT PRG or CRT:

```sh
python3 build_support/launcher_return_framework_plan.py --target d81 --disk PATH_TO_D81 --boot PATH_TO_PREBOOT --out logs/launcher-return-d81.json
python3 build_support/launcher_return_framework_plan.py --target easyflash --disk PATH_TO_COMPANION_D64 --boot PATH_TO_CRT --out logs/launcher-return-easyflash.json
python3 build_support/launcher_return_framework_plan.py --target ultimate --disk PATH_TO_ULTIMATE_D81 --out logs/launcher-return-ultimate.json
```

Run either VICE plan through the shared framework:

```sh
dotnet run --project ../agenticdevharness/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj -- run-plan --plan logs/launcher-return-d81.json --close-vice --no-tui
```

Run the physical plan from a fresh Terminal-owned/background shell, following
`AGENTS.md`:

```sh
dotnet run --project ../agenticdevharness/tools/vice_tasks_dotnet/src/ViceTasks.Binary/ViceTasks.Binary.csproj -- run-ultimate-plan --plan logs/launcher-return-ultimate.json --no-tui
```

These plans boot ReadyOS itself, never an individual app. Do not attach a
second monitor controller during framework runs. Accept only a final manifest
with `status: success`, no failed step, and no degraded steps.

## Contract checks

Passed: UCI protocol discipline, DMA compile/runtime gates, ReadyOS control-bank
layout, resume contracts, memory map, dynamic launcher catalog, documentation,
canonical shim HTML listings, and release directory ordering for all three
SKUs. The host C test exercises the production DMA-record implementation:
reset isolation, signature/version rejection, public header descriptors, and
preservation across normal header refresh and warm control-bank preparation.
