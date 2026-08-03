# ReadyOS Bank Refactor Implementation Log

## 2026-08-01 - baseline and architecture lock

- Preserved the dirty worktree and ran baseline builds/tests in an isolated
  source copy.
- Confirmed the current split:
  - physical `Skip`: schema-v4 global/control bank;
  - physical `Skip+1`: launcher snapshot;
  - `$C600-$C6FF`: resident physical allocation/type table;
  - `$C700`: resident magic and additional shared state;
  - `$C836-$C838`: 24-bit loaded snapshot bitmap;
  - `$C83B`: configured skip value used by arithmetic mapping.
- Locked the replacement: `Skip+1` becomes the combined launcher/metadata
  ReadyOS bank, `Skip` becomes allocatable, and token mappings become explicit.

## 2026-08-01 - schema v5 implementation and documentation

- Expanded every normal app snapshot to `$1000-$C7FF` (`$B800`) and moved the
  resume tail to `$B800-$FFFF` (`$4800`).
- Implemented the combined physical `Skip+1` ReadyOS bank and direct REU access
  for mapping, status, allocation types, clipboard metadata, hotkeys, app and
  resource registries, dependency lines, catalog text, audit, and runtime data.
- Reworked the exact 512-byte shim to resolve nonzero tokens through `$B940`
  and commit loaded/resumable status through `$BA40`; token 0 resolves directly
  to the ReadyOS bank stored at `$C83B`.
- Updated launcher, EasyFlash, ReadyShell, ReadyBASIC, REU Viewer, owned
  allocation, TUI, clipboard, and resume users. Retired monolithic micromodule
  sources remain in-tree but are compile-disabled and labeled.
- Removed the obsolete EasyFlash shim-bitmap writer. EasyFlash loader-only
  overlay metadata and debug staging now use `$CB20-$CB83`, outside the app
  snapshot and shim, before authoritative data is committed to REU.
- Added exhaustive Markdown/HTML status classification, canonical shim-source
  appendix synchronization, and exact HTML byte verification.
- `/bin/bash ./run.sh --build-all` passed for build `0.2.5D`, including all disk
  profiles, 1MB KFF2, and EasyFlash after the final loader cleanup.

## 2026-08-02 - runtime hardening and micromodule size lock

- Fixed the launcher catalog scratch-pointer lifetime: token/snapshot
  publication can reuse the catalog scratch area, so the filename is now
  acquired only after allocation and reacquired for DMA fallback.
- Restored launcher selection only after the ReadyOS-bank registry has been
  reconstructed and bounds-checked. Token 0 warm state remains a compact
  128-byte UI payload at ReadyOS-bank offset `$FE40`.
- Replaced duplicated TUI ReadyOS-byte DMA code in full REU clients with the
  two six-byte tail-jump entry points in `tui_readyos_alias.s`. The unique
  basename is intentional: an earlier same-basename `.c`/`.s` pair allowed
  GNU Make to regenerate and remove the assembler as an intermediate.
- Retained the safe C adapter and the lightweight standalone implementation as
  documented, compile-selected alternatives. Full REU clients use the alias;
  SysInfo and UCITest use the standalone no-BSS implementation.
- The final linker-map comparison records 87-88 bytes recovered per relevant
  full client. Launcher heap/headroom is now 5,132/5,244 bytes; ReadyShell
  heap/full-window headroom is 3,688/18,539 bytes after the obsolete RAM debug
  mirror was also removed.
- Hardened launcher and ReadyShell cross-app automation by clearing the KERNAL
  keyboard count and using VICE text-monitor `keybuf` for the lone RETURN that
  the binary-monitor helper could drop after a dump or app return.
- Retired five unused preload trace stores to app RAM `$C007-$C00C` and made
  reserved ABI entry `$C812` jump to the safe `$C9FF` `RTS`. The shim stays
  exactly 512 bytes and now has 129 verifier-enforced zero code-padding bytes,
  including a new 42-byte contiguous run at `$C8B6-$C8DF`.
- Removed ReadyShell's duplicate `$C7A0-$C7DF` / `$C7F0` diagnostic ring. Ring
  contents and head now exist only in the loader-assigned ReadyShell state
  bank; small live cursor/availability variables remain in BSS. This recovered
  another 96 bytes of regular ReadyShell resident heap without changing BSS.

## 2026-08-02 - aggregate completion and documentation lock

- Expanded the ReadyBASIC aggregate to cover all 26 maintained runtime suites,
  including screen-REU temporary storage, every graphics/sprite/sound phase,
  loaded-app behavior, and the final visual verification suite.
- Replaced remaining post-transition combined navigation/RETURN sequences with
  deterministic keyboard-buffer or direct logical-token actions where the
  harness had observable dropped-key races.
- Updated the ReuViewer F2-chain harness for the schema-v5 source of truth: it
  stages a 24-byte status image and DMA-writes it to ReadyOS-bank `$BA40`.
  This both preserves the two-app chain scenario on all-preloaded EasyFlash and
  proves the retired shim bitmap is no longer consulted.
- Regenerated ReadyBASIC spatial memory diagrams and public/private ReadyShell
  overlay inventories, rebuilt current Markdown-derived HTML, synchronized all
  complete commented shim sources, and reclassified/verified every HTML file.
- The retained memory comparison records per-app CODE/RO/init, DATA, BSS, heap,
  and full-window headroom before and after. ReadyBASIC is explicitly treated
  as a custom-assembler shape, not assigned fictional cc65 BSS/heap figures.
- Completed every regular and EasyFlash runtime suite, all release builds,
  static verifiers, host tests, HTML checks, whitespace checks, and the final
  no-deletion audit.
