# ReadyOS Documentation Index

This index separates current contracts from historical records. It was audited
against the `0.2.5` development tree on 2026-08-02.

An HTML counterpart is generated as `DOCUMENTATION_INDEX.html`. Every HTML file
under `docs/` and `privatedocs/`, the root completed-phase HTML report, and the
three ReadyBASIC HTML guides are classified by
`build_support/update_documentation_html_status.py` and carry a visible
`CURRENT DOCUMENT` or `PRESERVED SNAPSHOT / DESIGN` banner. The classification
changes presentation only: historical report bodies and measurements are not
deleted or rewritten into claims about a version they did not describe.

## Start Here

- [`../README.md`](../README.md): project overview, current SKUs, app catalog,
  runtime summary, and supported build/run entry points.
- [`ultimate_dos_dma_loading.md`](ultimate_dos_dma_loading.md): opt-in C64
  Ultimate DOS DMA loading, fallback behavior, configuration, and verification.
- [`ReadyOS_SHIM_ARCHITECTURE_0.2.5.md`](ReadyOS_SHIM_ARCHITECTURE_0.2.5.md):
  current resident-shim and schema-v5 ReadyOS-bank architecture, including which
  state remains resident, which state lives in physical `Skip+1`, and which copy is
  authoritative for each operation.
- [`../privatedocs/top_level_md/MEMORY_MAP.md`](../privatedocs/top_level_md/MEMORY_MAP.md):
  canonical detailed RAM, REU, shim, and per-app memory contract.
- [`../privatedocs/top_level_md/SHIM_PLAN.md`](../privatedocs/top_level_md/SHIM_PLAN.md):
  current resident-shim ABI and switching flow (the historical filename is
  retained).

## Current Subsystem Documentation

- ReadyShell: [`ReadyShellArchitecture.md`](ReadyShellArchitecture.md),
  [`ReadyShellHostTesting.md`](ReadyShellHostTesting.md), and the generated
  [`readyshell_overlay_inventory.md`](readyshell_overlay_inventory.md).
- ReadyBASIC: [`../src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md`](../src/apps/readybasic/READYBASIC_CURRENT_DESIGN.md),
  graphics and sound command designs, lifecycle/REU architecture, module-making
  guide, and the examples listed in the root README.
- EasyFlash: [`reports/easyflash_boot_flow.md`](reports/easyflash_boot_flow.md).
- File formats: the `*_format.md` and `*_seq_format.md` documents in this
  directory.
- C64 Ultimate DMA research evidence:
  [`../ULTIMATEDOS_DMA_LOADING_LESSONS_LEARNT.md`](../ULTIMATEDOS_DMA_LOADING_LESSONS_LEARNT.md)
  and [`../probes/uci_dma/README.md`](../probes/uci_dma/README.md).

## Generated Reports

- `make readyshell-overlay-report` refreshes the public/private ReadyShell
  overlay Markdown and HTML reports from maps and profile metadata.
- `make readybasic-memory-report` refreshes the ReadyBASIC proportional memory
  report and diagrams.
- `make easyflash-report` reports the generated cartridge layout.
- `python3 build_support/report_app_headroom.py` emits current per-app segment
  endpoints and free bytes from `obj/*.map`.

Generated reports are current only for the artifacts in the working tree. A
dated stability report or an `agentworking/` log is evidence from that run, not
a live architecture contract.

After changing Markdown/HTML documentation, regenerate current HTML
counterparts and run:

```sh
python3 build_support/update_documentation_html_status.py
python3 build_support/sync_shim_documentation.py
python3 build_support/verify_shim_html_source.py
```

The script fails if any covered HTML document is not explicitly classified,
preventing a new report from silently appearing without freshness status. The
shim-source verifier discovers every HTML `<pre>` block containing
the complete resident shim, compares all `.byte` directives with
`src/boot/readyos_shim.inc`, and rejects retired lookup annotations.

## Historical Material Policy

- `Releases/<version>/` documents describe the artifacts shipped in that
  version and are intentionally not rewritten to match later source.
- `agentworking/`, dated stability reports, files explicitly named `OLD_*`, and
  completed/refactor lessons retain their original observations. New notes may
  mark a claim as superseded, but the evidence itself is preserved.
- Current decisions belong in the root README, `docs/`, the canonical private
  memory/shim documents, or the relevant app's current-design document.
