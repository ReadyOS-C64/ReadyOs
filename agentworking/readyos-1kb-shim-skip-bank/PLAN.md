# ReadyOS 1KB Shim / ReadyOS-at-Skip Refactor

Baseline commit: `1644b22` (`Unify ReadyOS bank state and refresh documentation`)

## Required final contract

- The complete resident ReadyOS shim area is `$C600-$C9FF` (`$0400`, 1024 bytes).
- The existing public jump table and data ABI remains fixed at `$C800-$C9FF`.
- `$C600-$C7FF` is reserved shim expansion space and is not linked, allocated,
  snapshotted, or treated as scratch by applications.
- The app snapshot is `$1000-$C5FF` (`$B600`, 46,592 bytes).
- Physical `Skip` is the combined ReadyOS bank. With `Skip=0`, physical bank 0
  is the ReadyOS bank.
- Physical `Skip+1` is the first dynamic allocation candidate; token 0 resolves
  directly to physical `Skip`; nonzero tokens resolve through the ReadyOS-bank
  mapping table.
- The ReadyOS bank contains the launcher snapshot at `$0000-$B5FF` and schema-v5
  state at `$B600-$FFFF`.
- ReadyBASIC preserves its custom assembler/linker shape and does not use
  `$C600-$C9FF`.
- ReadyShell retains its `$8E00-$C5FF` overlay ABI.
- Disk, EasyFlash, and Ultimate DMA launch/switch/resume behavior must agree.

## Verification evidence required

- Before/after linker, code/data/BSS, heap, and full-window headroom reports for
  every app and both regular/EasyFlash launchers and ReadyShell builds.
- Exact 1024-byte disk/EasyFlash shim identity and canonical commented source
  verification, including HTML full-source copies.
- Static memory, snapshot/resume, ReadyOS-bank schema, dynamic allocation,
  ReadyBASIC-shape, documentation, and no-deletion gates.
- All release-profile builds and EasyFlash packing/smoke verification.
- Full regular and EasyFlash ReadyBASIC suites.
- ReadyShell host, overlay, cross-app resume, and UI automation suites.
- Core UI/app switching, hotkey, clipboard, owned-bank, unload, and resume suites.
- Ultimate 64 UCI/DMA loading and cross-app suites launched through the required
  Terminal-owned background workflow.

## Safety rules

- Do not delete source, documentation, reports, or release artifacts.
- Preserve baseline evidence and append test results to `TEST_LOG.md`.
- Build ReadyOS only through `run.sh`/`run.ps1` and the established aggregate
  Makefile test targets; never launch a single app directly.
