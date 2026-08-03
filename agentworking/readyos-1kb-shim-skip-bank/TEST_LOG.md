# ReadyOS 1KB Shim / ReadyOS-at-Skip Test Log

## Baseline

- Known-good working state committed as `1644b22`.
- Fresh pre-change linker/headroom report: `headroom_before.json`.
- Pre-change contract: app snapshot `$1000-$C7FF` (`$B800`), 512-byte shim
  `$C800-$C9FF`, ReadyOS bank at physical `Skip+1`.

## Implementation and build verification

- `/bin/bash ./run.sh --build-all`: PASS for all release SKUs and EasyFlash.
- Generated release version: `0.2.5L`.
- `python3 build_support/verify_readyos_shim.py`: PASS; canonical resident shim is
  1024 bytes, disk boot clears `$C600-$C7FF` and installs the `$C800-$C9FF`
  ABI, and EasyFlash packages the full resident image.
- `python3 build_support/verify_reu_control_bank.py`: PASS.
- `python3 build_support/verify_memory_map.py`: PASS; expected warning only for
  ReadyShell overlay 6 ending at `$C5F4`.
- `python3 build_support/verify_dynamic_launcher.py`: PASS.
- `python3 build_support/verify_resume_contract.py`: PASS.
- `python3 build_support/verify_readybasic_plugin.py`: PASS.
- `python3 verify.py`: PASS.
- `python3 build_support/verify_shim_html_source.py`: PASS; all five complete
  HTML source listings contain the 175 canonical `.byte` directives.

## Host tests

- `python3 build_support/editor_host_smoke.py`: PASS, headroom 12,194 bytes.
- `python3 build_support/tasklist_host_smoke.py`: PASS, headroom 6,398 bytes.
- `python3 build_support/simplefiles_host_smoke.py`: PASS.
- `make readyshell-host-tests`: PASS for parser, VM, C64 overlay VM, REU/value,
  and serialization tests; compiler warnings are unchanged unused static helpers.

## Regular VICE/UI automation

- Full ReadyBASIC aggregate (26 suites): PASS; manifest
  `logs/vice_auto_20260802_190250/manifest.json`.
- ReadyBASIC hotkey, cross-app resume, second-entry Editor return, loaded-apps,
  and keyboard regression probes: PASS; manifests `...184119`, `...185151`,
  `...185342`, `...185924`, and `...185052` respectively.
- Launcher ReadyOS-bank/schema state probe: 28/28 PASS; manifest `...191057`.
- QuickNotes owned-REU lifecycle before unload: 15/15 PASS; manifest `...191156`.
- QuickNotes unload/release lifecycle: 17/17 PASS; manifest `...191215`.
- ReadyShell Editor -> ReadyBASIC -> Editor -> ReadyShell overlay/resume sequence:
  33/33 PASS on unchanged retry; manifest `...191608`.
- The first keyboard attempt timed out and the first ReadyShell attempt dropped
  its final `EXIT` key. Both partial failures are retained in `logs/`; fresh,
  unchanged reruns passed and are the manifests cited above.

## EasyFlash/cartridge automation

- `make easyflash-verify`: PASS for layout, static contract, full 1KB shim,
  schema v5, preload image, and launcher verification.
- `make easyflash-preload-verify`: PASS.
- Full ReadyBASIC visual/command/heap/procedure suite: 188/188 PASS; manifest
  `logs/vice_auto_20260802_204002/manifest.json`.
- ReadyBASIC second-entry Editor probe: 194/194 PASS; manifest
  `logs/vice_auto_20260802_203756/manifest.json`.
- Ten ReadyBASIC <-> Editor switch/resume cycles: 265/265 PASS; manifest
  `logs/vice_auto_20260802_202538/manifest.json`.
- ReadyShell Editor -> ReadyBASIC -> Editor -> ReadyShell overlay/resume sequence:
  35/35 PASS; manifest `logs/vice_auto_20260802_204753/manifest.json`.
- ReadyBASIC REUViewer/F2 chain: 70/70 PASS; manifest
  `logs/vice_auto_20260802_202447/manifest.json`. This found and corrected a
  stale test-only assumption that the ReadyOS bank was `Skip+1` and that token
  status lived at the previous schema offset. The probe now derives both values
  from generated/source configuration.
- Aggregate EasyFlash plans also passed demo, repeat/label, lifecycle, module,
  plugin, program, RBTEST1, resume, screen-REU, state, large-variable, F4/F2
  hotkey, and keyboard coverage. Their passing manifests span `...193240`
  through `...201921`; all failed cold-start attempts and passing fresh-process
  retries remain preserved in `logs/`.

## Headroom result

- Detailed before/after data: `headroom_before.json`, `headroom_after.json`, and
  `headroom_comparison.md` in this directory.
- Every conventional app gives the reserved lower shim half its intended 512
  bytes; code/data/BSS are otherwise stable.
- Tightest conventional application is Dizzy: 1,468-byte heap and 1,580 bytes
  of raw window headroom.
- ReadyBASIC retains 1,025 bytes to `$C5FF` in its custom layout.
- ReadyShell retains its 3,688-byte heap; overlay 6 ends at `$C5F4` and therefore
  has 11 bytes of overlay-address headroom without consuming that heap.

## Safety audit so far

- The release builder's normal cleanup briefly removed old tracked versioned
  release files. They were detected immediately and restored from the checkpoint.
- `git status --short` contains no tracked deletions after restoration.
- Historical documentation is preserved; stale layouts are explicitly marked as
  historical rather than removed.

## Final recheck pass

- `make verify`: PASS after the complete source and documentation update. This
  reran the aggregate memory-map, ONCE/BSS, 1 KB shim, schema-v5, dynamic
  launcher, resume, ReadyBASIC-shape, and documentation gates.
- `python3 build_support/verify_documentation_contract.py`: PASS for 14 live
  contract documents, all retained historical-contract markers, and all 38
  classified HTML documents. The corpus audit subsequently standardized six
  more historical Markdown reports; the final count is now 31 historical
  markers.
- `python3 build_support/verify_shim_html_source.py`: PASS for five complete
  commented HTML shim listings, each matching all 175 canonical `.byte`
  directives.
- Host checks rerun unchanged: Editor, Tasklist, SimpleFiles, and the complete
  ReadyShell host suite all PASS.
- Final report regeneration caught and corrected a generator-level HTML status
  regression: `readybasic_memory_report.py` and `readyshell_overlay_report.py`
  now emit their exact current-document banners themselves. The regenerated
  public/private reports and the 38-document classification gate all PASS.
- Canonical `/bin/bash ./run.sh --build-all`: PASS with exit code 0 for the
  complete 0.2.5Y disk-SKU matrix and EasyFlash artifacts. The post-build
  preservation audit found zero tracked deletions.
- Regular VICE empty-clipboard return control: 12/12 PASS; manifest
  `logs/vice_auto_20260802_212333/manifest.json`.
- Final regular-D81 non-empty clipboard lifecycle: 32/32 PASS with zero degraded
  steps; manifest `logs/vice_auto_20260802_222311/manifest.json`. Editor copied
  `CLIPTEST42`, Clipboard Manager reported exactly one item and previewed the
  text, Editor resumed with its original contents, and paste produced
  `ZCLIPTEST42`. The run also dumped the complete resident `$C600-$C9FF` shim.
- Retained failed clipboard diagnostics used either the Ultimate-DMA artifact or
  an unreliable isolated harness Return injection. The passing plan uses the
  established queue-clear plus VICE `keybuf` method from the second-entry suite;
  no ReadyOS source change was needed.

## Ultimate 64 acceptance

- Editor direct DMA launch and return: PASS
  (`c64u-dma/editor-direct-dma-return/status`).
- Editor F3 load-selected DMA path: PASS
  (`c64u-dma/editor-load-selected/status`).
- F1 load-all followed by ReadyShell overlay execution: PASS on unchanged retry
  (`c64u-dma-retry/loadall-readyshell-overlay-smoke/status`).
- F5 manifest load followed by SidetRIS launch: PASS
  (`c64u-dma-retry2/manifest-sidetris/status`).
- The first ReadyBASIC load-selected retry remained blank before ReadyOS boot
  and ended `READYOS_BOOT_FAIL`; it never reached or exercised ReadyBASIC. The
  failure is retained under `c64u-dma-retry2/readybasic-load-selected/`.
- ReadyBASIC F3 load-selected DMA path: PASS on an isolated unchanged retry
  (`c64u-readybasic-retry3/readybasic-load-selected/status`). The run proved
  launcher DMA readiness, loader completion, `DMA:ON`, and the final
  ReadyBASIC screen.
- Editor -> ReadyBASIC -> ReadyShell hardware lifecycle: 30/30 PASS with zero
  degraded steps; manifest `logs/ultimate_auto_20260802_214758/manifest.json`.
  The run modified and suspended Editor, launched and exercised ReadyBASIC,
  returned through the launcher, then loaded ReadyShell and verified `VER`,
  `LST`, and `CAT` overlay execution before dumping the final app/shim state.
- Stable-path DMA build for the ReadyIRC suite: PASS via
  `LAUNCHER_DMA_LOAD=1 /bin/bash ./run.sh --profile precog-d81 --config
  build_support/c64u_dma_acceptance/precog-d81-dma-valid.ini --build-only`.
  Artifact `readyos-v0.2.5w-d81.d81` uses `reu_bank_skip=32` and launcher path
  `/usb1/readyos.d81`.
- ReadyIRC Ultimate 64 lifecycle: 82/82 PASS with zero degraded steps; manifest
  `logs/ultimate_auto_20260802_215831/manifest.json`. It covers setup/input
  validation, lowercase charset, mixed-case network traffic, append/scroll
  rendering, history stability, `NAMES`, channel switching, live suspend/resume
  without reconnect, stale-socket reconnect, intentional disconnect, retained
  settings/channel, and final full-shim dump. The fixture independently passed
  all protocol assertions: three registrations, exact mixed-case PONG, ordered
  PART/JOIN, expected NAMES requests, switched-channel PRIVMSG, and two QUITs.
