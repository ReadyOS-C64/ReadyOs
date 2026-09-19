# ReadyOS Documentation Index

This index separates current contracts from historical records. It was audited
against the `0.5` development sources on 2026-09-18. That tree is based
on the audited, production-stamped `0.2.5` release state.

An HTML counterpart is generated as `DOCUMENTATION_INDEX.html`. Every HTML file
under `docs/` and `privatedocs/`, the root completed-phase HTML report, and the
three ReadyBASIC HTML guides are classified by
`build_support/update_documentation_html_status.py` and carry a visible
`CURRENT DOCUMENT` or `PRESERVED SNAPSHOT / DESIGN` banner. The classification
changes presentation only: historical report bodies and measurements are not
deleted or rewritten into claims about a version they did not describe.

## Start Here

- [ReadyBASIC examples and commands](readybasic_reference.md)
  ([styled HTML](readybasic_reference.html)): all 45 source examples, the current
  built-in inventory, eight on-demand media commands and per-SKU availability.
- [App hardware and REU requirements](app_requirements.md): Ultimate-only
  functions, app-owned versus shared REU, and the standalone release plans.
- [September audit](documentation_audit_2026-09.md): corrections and issues that
  need packaging, code or validation beyond documentation.
- [Private current-state guide](../privatedocs/reports/readybasic_current_state.md):
  new media/module lifecycle, product direction and retained-evidence policy.

- [`../README.md`](../README.md): project overview, current SKUs, app catalog,
  runtime summary, and supported build/run entry points.
- [`ultimate_setup.md`](ultimate_setup.md): first-run flow, controls, safe
  config commit, prerequisites, and automation contract for the Ultimate D81.
- [`ultimate_dos_dma_loading.md`](ultimate_dos_dma_loading.md): C64 Ultimate
  DOS DMA loading, profile/runtime gates, fallback behavior, and verification.
- [`uci_tester.md`](uci_tester.md): UCI Tester controls, protocol rules,
  selectable prefills, safety notes, and real-world DOS/network/HTTP workflows.
- [`ReadyOS_SHIM_ARCHITECTURE_0.5.md`](ReadyOS_SHIM_ARCHITECTURE_0.5.md):
  current resident-shim and schema-v5 ReadyOS-bank architecture, including which
  state remains resident, which state lives in physical `Skip`, and which copy is
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
  guide, and the complete reference linked above. The
  [Orbital Echoes walkthrough](readybasic_orbital_echoes.md),
  [media learnings](readybasic_media_learnings.md), and
  [Ultimate speed-policy research](readyos_ultimate_speed_policy.md) separate
  current contracts from dated test results and unimplemented OS-wide policy.
- EasyFlash: [`reports/easyflash_boot_flow.md`](reports/easyflash_boot_flow.md).
- File formats: the `*_format.md` and `*_seq_format.md` documents in this
  directory.
- C64 Ultimate DMA research evidence:
  [`../ULTIMATEDOS_DMA_LOADING_LESSONS_LEARNT.md`](../ULTIMATEDOS_DMA_LOADING_LESSONS_LEARNT.md)
  and [`../probes/uci_dma/README.md`](../probes/uci_dma/README.md).
- C64 Ultimate interactive diagnostics: [`uci_tester.md`](uci_tester.md) and
  its rendered [`uci_tester.html`](uci_tester.html) counterpart.

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

For a documentation-only release commit that excludes local media rebuilds,
use `refresh_release_docs.py --manifest-ref HEAD` (also with `--check`) to keep
artifact names aligned with committed manifests. Private HTML is refreshed
only when its local, untracked Markdown source is available.

```sh
python3 build_support/build_readybasic_reference.py
python3 build_support/refresh_release_docs.py
python3 build_support/render_current_docs.py
python3 build_support/update_documentation_html_status.py
python3 build_support/sync_shim_documentation.py
python3 build_support/verify_documentation_contract.py
python3 build_support/verify_documentation_links.py
python3 build_support/verify_readybasic_documentation.py
python3 build_support/verify_shim_html_source.py
python3 build_support/render_current_docs.py --check
python3 build_support/refresh_release_docs.py --check
```

The reference generator preserves every `.bas` listing verbatim and refuses
unknown built-in commands without an explanation. The release-document refresh
uses each existing manifest's artifact names without rebuilding/renaming media.
The renderer updates only named current counterparts, never historical reports.

The documentation-contract verifier checks the current RAM/REU statements,
the 1 MB SKU bank budget, retained-history supersession markers, and the HTML
classification. Classification fails if any covered HTML document is not
explicitly categorized, preventing a new report from silently appearing
without freshness status. The shim-source verifier discovers every HTML `<pre>` block containing
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

## ReadyBASIC 0.5 RC2 sample packages

[Disk sample commands and loading](../src/apps/readybasic/READYBASIC_SAMPLE_MODULES.md)
covers LDMOD, PAUSE, the disk-only scalar proofs, and slot/overlay demonstrations.
