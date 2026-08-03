# REU Refactor Lessons Learnt

## 2026-08-02 final authority cleanup

- ReadyShell's `$C7A0-$C7DF` / `$C7F0` diagnostic mirror was the last live
  duplicate found in the reclaimed tail. Diagnostics now have one durable
  source in the loader-assigned ReadyShell state bank; only a live cursor and
  availability flag remain in BSS.
- Five unconsumed shim preload trace stores into `$C007-$C00C` were retired,
  and reserved entry `$C812` became a safe no-op. The exact 512-byte shim now
  has 129 verifier-checked padding bytes, with a 42-byte largest run.
- Full REU clients share `reu_mgr_dma.c` through zero-frame assembly aliases;
  lightweight apps retain a standalone no-BSS byte helper. A unique assembler
  basename is required so Make cannot regenerate it from the retained C
  reference adapter as an intermediate.

## 2026-08-01 Schema-v5 completion

- Physical `Skip+1` is now the combined ReadyOS bank and single metadata
  authority; it contains the launcher snapshot plus schema-v5 state.
- The earlier `$C600-$C7FF` RAM mirror and separate control bank were useful
  migration steps, but are superseded. Physical `Skip` is reclaimed as the
  first dynamic bank and tokens use explicit mapping/status tables.
- Focused micromodules prevented broad app BSS growth; the complete 24-map
  comparison is in `agentworking/readyos-bank-refactor/headroom_comparison.md`.
- The shim stayed exactly 512 bytes, but changing one instruction length proved
  why byte-anchor verification is mandatory for every disk and cartridge build.

## 2026-06-02

- The refactor must be treated as a whole-system contract change. Old app,
  shim, EasyFlash, overlay, and micromodule binaries do not need compatibility
  support, but stale generated artifacts must be invalidated and rebuilt.
- Do not put bank-0 mirror/control code into `reu_mgr_init.c` until the size
  impact is proven. Many normal apps link that file, so doing so would risk
  broad code growth.
- First implementation target is mirror/audit, not authority: `$C600-$C7FF`
  remains runtime truth while logical bank `0` records are initialized and
  checked.
- ReadyShell cartridge preload already proves the desired dependency concept:
  app resources can be loaded before app entry. The first step is to describe
  those dependencies as generated records, not to redesign ReadyShell.
- The fixed-resource mirror must describe every fixed resource that the
  resident bank table marks. Missing ReadyShell scratch `$48` from the compact
  records made the mirror internally incomplete even though the bitmap table
  was correct.
- EasyFlash verification must be rerun after launcher-control changes because
  the cartridge launcher is a separate binary and preload behavior is separate
  from the disk launcher path.
- A passed shim verifier is the hard guardrail for this phase: the bank `0`
  mirror work can change launcher/reuviewer behavior, but must not grow or
  reinterpret the `$C800-$C9FF` shim ABI.
- Runtime REU-content verification should use C64-side code or a proven VICE
  monitor I/O-address-space sequence. A quick monitor-side attempt to poke
  `$DF01-$DF0A` did not reliably change the live REU transfer registers, so it
  is not a sound basis for accepting the bank `0` runtime contents.
- ReadyBASIC is currently the tightest app-window binary in the generated
  report, with 1031 bytes of headroom. Any later shared-library change that
  links into normal apps needs a before/after report, not just a successful
  rebuild.
- A 64-entry launcher catalog has a real RAM cost. The first dynamic
  implementation accidentally doubled that cost with a resident
  `LauncherCatalogCacheV1` resume copy and dropped launcher headroom by 8583
  bytes. Saving the real arrays as segmented REU resume payloads recovered
  more than 5.5KB and left the accepted launcher delta at 3060 bytes.
- Low logical app banks cannot be treated as permanently unavailable just
  because the bank table marks their physical slots `REU_RESERVED` at boot.
  For the dynamic app allocator, low logical banks `1..23` remain valid app
  snapshot candidates so the existing loaded-bank bitmap and switch behavior
  stay useful for normal catalogs.
- Do not implement manifest dependency loading by adding a broad runtime parser
  to the launcher. The stable path is generated dependency/resource records
  first, then ReadyShell and ReadyBASIC consumers, then runtime manifest syntax
  only when the binary contract is boring.
- The shim bitmap remains a three-byte low-bank compatibility field. Any
  cartridge or dynamic path that uses logical banks above 23 must record loaded
  state outside the shim bitmap and must not expect `set_bitmap` to represent
  those banks.
- Cartridge preload has its own correctness boundary. The booter can stash
  logical banks above 23, but the launcher must explicitly mark embedded
  preloads as loaded and mirror their physical banks into `$C600`/bank `0`.
- Load-all UI code that was harmless with 23 app slots can become unsafe with
  64. Progress/status displays must wrap or window visible rows rather than
  writing unique rows for every catalog entry.
- Normal app impact stayed at 0 or 1 byte only because dynamic allocator and
  bank `0` mirror code stayed out of shared app libraries. Keep that boundary.
- Unload belongs to the launcher or future ReadyOS manager. The shim remains a
  direct-bank transfer primitive and must not grow owner/free-list policy.
- ReadyShell overlay cache banks can be dynamic without making ReadyShell own
  allocation. The launcher/loader should allocate the `rsovl` banks, write the
  metadata block at offset `$80F0` inside the assigned ReadyShell state bank,
  and let ReadyShell consume only the bank ids it needs.
- ReadyShell's scratch/value arena should be treated like the overlay banks:
  loader-owned, resource-recorded, and unloadable. Seeding ReadyShell's `$CFF2`
  state-bank cache before entry avoids a wrong-bank scan if a future catalog
  ever has more than one `REU_RS_SCRATCH` owner.
- The `$CFF2` state-bank cache is deliberately outside the shim ABI. `$CFF1`
  was already ReadyShell's command-session epoch byte, so do not reuse nearby
  shim-adjacent bytes without checking both launcher and app high-RAM runtime
  allocations.
- Keep temporary diagnostics out of the final ReadyShell resident path unless
  they materially improve field support. The final dynamic state-bank change
  costs ReadyShell 120 bytes of CODE, 0 bytes of BSS/RODATA/DATA, and 120 bytes
  of resident heap; keeping the boot-screen numeric diagnostic would have added
  extra code/RODATA for a problem that the targeted VICE suite now covers.
- Preserve ReadyShell's slot geometry when changing ownership. Keeping
  `+$0000`, `+$3800`, `+$7000`, `+$A800` avoided a broad ReadyShell rewrite and
  confined app impact to metadata read/registry patching.
- Do not use the shim app preload path for ReadyShell overlay sidecars. They
  are PRGs for the `$8E00-$C5FF` overlay window, not normal app snapshots, so
  the disk launcher needs a small streaming loader into the assigned REU slots.
- Once a resource set is loader-owned, do not leave an app-side fallback that
  silently recreates ownership. Removing ReadyShell's overlay self-loader made
  metadata failure explicit and improved resident headroom.
- Cartridge/EasyFlash dynamic banks must be generated, not guessed. The boot
  assembly can still preload ReadyShell overlays, but its cache bank constants
  now come from generated layout artifacts.
- Historical phase note: the ReadyShell `$48` scratch/state/value bank
  intentionally stayed fixed while only overlay cache banks moved. That kept
  the first resource refactor small enough to verify. This was later superseded:
  ReadyShell scratch/state/value/CAT/diagnostics now live in a loader-assigned
  state bank.
- ReadyBASIC core/code banks can be made dynamic without adding shim ABI bytes:
  the launcher marks two assigned physical banks as `REU_RB_CORE` and
  `REU_RB_CODE` in `$C600`, and ReadyBASIC resolves those types at startup.
  Keep this as a small micromodule contract, not a broad app-side allocator.
- Do not let the shared REU sync code repopulate retired fixed banks. Removing
  fixed `$44/$45` writes from `reu_mgr_init.c` and the bank-0 mirror is what
  makes the ReadyBASIC change real rather than cosmetic.
- ReadyBASIC resource unload belongs to the launcher. Free the app snapshot and
  its `rbcore` banks together; do not grow the shim into an owner/free-list
  engine.
- The ReadyBASIC resolver must tolerate visible metadata timing. Resolving
  `REU_RB_CORE`/`REU_RB_CODE` from `$C600` is the fast path, but the logical
  bank `0` bank-type mirror fallback is what made the launcher/shim handoff
  robust without adding shim bytes.
- The accepted ReadyBASIC dynamic-bank delta is intentionally tiny: ReadyBASIC
  app-window headroom moved from `1031` to `1029` bytes, while launcher
  headroom moved from `5075` to `4637` bytes because the launcher owns the
  `rbcore` load/unload path.
- For bank `0` registry metadata, array-copy beats formatted records in the
  launcher. A linked-list/record serializer was attractive but cost about
  `2.3KB` of launcher headroom and pulled almost `2KB` into reuviewer before
  splitting. The accepted shape copies existing normalized launcher arrays into
  separated REU blocks and leaves richer name-linked records as reserved design
  space until there is a concrete consumer.
- Rich resource ownership records are useful, but keep them out of the hot shim
  mirror path. The v3 compromise is `$0300/$0500/$0900` for cheap array copies
  and `$0A00/$0E00` for slower loader/viewer-only relationship data.
- Disk ReadyShell overlay placement can be config-driven without making
  ReadyShell own the parser. The launcher parses one compact dependency line,
  writes small v4 `OV` metadata, and ReadyShell consumes only `(bank, offset)`
  records.
- Bank-0 dependency writers must be alias-safe. The launcher sometimes writes
  the same dependency buffer it is holding; zeroing the destination before
  copying erased the source and caused `rs fail count 0` in the ReadyShell
  cross-app VICE probe. The writer now copies before clearing the tail, and the
  static verifier checks for that pattern.
- Preserve specific launcher failure notices while debugging resource loading.
  A generic "app resources failed" message hid whether ReadyShell failed bank
  allocation, dependency lookup, parse, load, or record writes.
- EasyFlash overlay metadata verification must read the full VICE monitor
  memory region, not just the first 16-byte row. The v4 `OV` block is 36 bytes
  at `$C760`, so verifier comparisons should use region assembly.
- EasyFlash boot metadata must use the same byte order as the disk launcher and
  ReadyShell runtime: `(bank, offset_lo, offset_hi)`. The first generated v4
  boot writer put non-zero offset high bytes in the low-byte slot; the cartridge
  smoke test caught it.
- The launcher is now the tight binary in this part of the system. The v3 rich
  records are acceptable because normal app headroom is unchanged, ReadyBASIC is
  unchanged, and launcher still has about 1KB of app-window headroom. Treat any
  future launcher parser feature as expensive until measured.
- Large launcher catalog text belongs in global REU, not resident BSS. Moving
  64 app names/descriptions/file tokens to logical bank `0` at
  `$3000/$3800/$4200`, while keeping a 12-row RAM name cache and one shared
  text/file scratch buffer, recovered 5508 bytes of launcher headroom without
  changing the TUI hotkey module, shim ABI, app.config syntax, or manifest
  syntax. The key is to cache visible menu rows, not to perform tiny REU reads
  for every character draw.
- Cartridge verification must not run concurrently with other VICE probes that
  perform prelaunch cleanup. The dotnet probe harness kills stale `x64sc`
  processes before launching, which can interrupt monitor-script smoke tests and
  produce a false missing-dump failure.
- Cartridge ReadyBASIC first-entry bugs are best fixed in ReadyBASIC's own
  runtime validation/cold-start path. The launcher should provide assigned
  resource banks; it should not become a BASIC zero-page/CHRGET/TXTPTR
  initializer. Using BASIC ROM `$E3BF` and `$A68E` kept the fix small and kept
  cartridge conditionals out of the launcher/shim contract.
- Generated cartridge metadata should be treated like a compiled manifest.
  EasyFlash may remain static, but the emitted ReadyShell overlay rows and bank
  `0` rich records must be generated from the same layout source as the preload
  image, not copied as independent hard-coded tables.
- The shim can depend on REU bank `0` without growing beyond 512 bytes if the
  contract is a bounded byte lookup, not a parser. The accepted pattern is:
  token `0` maps directly to launcher `skip+1`; non-zero tokens fetch one byte
  from `$2F00 + token` in the ReadyOS global/control bank into `$C83D`; then
  the existing physical-bank setup path runs unchanged.
- Do not recreate old `REU_RESERVED` placeholders for clear shim bitmap bits.
  They were a display/allocator artifact, not ownership. Clear old reserved
  entries to `REU_FREE`, preserve explicit launcher `REU_APP_STATE`
  allocations until unload/free, and let resource/app ownership records in
  bank `0` explain who owns a bank.
- `skip+26` is retired as a resource/dynamic allocation base. Start scanning at
  `skip+2`; the old `skip+3` start only existed while the retired launcher
  overlay reserve occupied `skip+2`. Trust the allocation table to skip system banks,
  app snapshots, and resources already in use. This recovered app-side code in
  the split REU manager while costing launcher only 112 bytes for the
  lookup-page mirror.
- ReadyShell diagnostics and CAT did not need a separate fixed support bank.
  The better shape is a small reserved tail inside the loader-assigned
  ReadyShell state bank (`+$7DE0-+$7FFF`) plus shared command scratch in the
  front. Command overlays execute serially, so CAT can reuse the same scratch
  region as LST/LDV/STV/PUT/ADD without a distinct ownership type, while unload
  stays simple because all ReadyShell scratch/diagnostic state belongs to the
  ReadyShell state resource.
- Physical REU size must be launcher/system-owned, not viewer-local. The
  launcher should probe once at startup, mark the unavailable physical tail in
  `$C600-$C6FF`, mirror that to logical bank `0`, and publish the encoded count
  in the `RCB0` header. REU Viewer should only consume those facts; otherwise a
  smaller REU can show impossible totals or let display logic drift from
  allocation policy.
- Keep the destructive alias probe out of diagnostic apps. Splitting
  `reu_phys.c` table helpers from launcher-only `reu_phys_probe.c` made the
  shared cost small: launcher pays one BSS byte for the probe scratch, while
  REU Viewer avoids probe BSS and only links display/table helpers.
- Physical-size work is not verified by REU Viewer screenshots alone. The
  final confidence point is disk plus EasyFlash VICE coverage, because the
  cartridge SKU has a generated preload path and static resource metadata that
  can drift from the disk launcher if it is not exercised.
