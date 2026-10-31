# ReadyOS documentation

| Task | Guide |
| --- | --- |
| Boot ReadyOS or choose a disk | [Quick start](../README.md#getting-started) and [release folders](../Releases/0.5/README.md) |
| Configure Ultimate disk loading | [First-run SETUP](ultimate_setup.md) |
| Pick an app | [App catalog](../README.md#app-catalog) and [hardware requirements](app_requirements.md) |
| Work in ReadyShell | [Tutorial](../src/apps/readyshell/ReadyShelltutorial.md) and [command guide](../src/apps/readyshell/README.md) |
| Run ReadyBASIC programs | [Examples and commands](readybasic_reference.md) |
| Write a ReadyOS app | [Contributor notes](../CONTRIBUTING.md) and [shim ABI](ReadyOS_SHIM_ARCHITECTURE_0.5.md) |
| Build a ReadyBASIC module | [Package layout and examples](../src/apps/readybasic/READYBASIC_SAMPLE_MODULES.md) and [adding a command](../src/apps/readybasic/READYBASIC_MAKING_COMMAND_GUIDE.md) |
| Set up an OS bridge | [C64OS](c64os/ultimate.md), [GDOS64](gdos64/ultimate.md) or [GEOS 2.0](geos20/ultimate.md) |

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
