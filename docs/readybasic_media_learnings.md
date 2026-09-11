# ReadyBASIC MEMCAP / media experiment

Branch: `codex/readybasic-memcap-sidplay`. Experimental regular D81 profile.

## Contract and commands

`MEMCAP(address)` is built in and does not require a disk module. Address is a
page-aligned, exclusive BASIC ceiling, at most 40960 (`$a000`). It validates the
program/variable/array end and requires an empty dynamic string heap when the
ceiling changes. It does not call CLR or relocate live strings. Numeric
variables and arrays survive. Repeating the same cap is idempotent, even with
heap strings. Invalid changes leave memory pointers unchanged (`?RB ERROR 14`).
Use it near the start of the program, before creating strings. `CLR` is an
explicit way to discard variables before a later cap change. `FRE(0)` and the
actual BASIC heap ceiling change together. The default remains 30013 bytes
free in an empty workspace, with the program at `$2ac1`.

`ZMODLD("rbm.media",M%)` loads five commands, without consuming BASIC RAM:

| Command | Meaning |
|---|---|
| `MUSTUNE("rb.summer")` | Load fixed-address PSID into previously capped RAM; install driver, but don't play. |
| `MUSPLAY(1)` | Initialize/restart one-based subtune and start raster-driven playback. |
| `MUSHALT()` | Detach IRQ, silence all SID registers, keep loaded tune and reservation. |
| `MUSDROP()` | Halt and release the tune's lifetime lock; it does not raise MEMCAP itself. |
| `RSCFILE("name")` | Load bounded generic RBR1 resource into capped RAM, with no music requirement. |

These are statement commands. `MUSDROP():MEMCAP(40960)` returns the arena to
BASIC if the heap is empty. The module stays registered in REU. BASIC END/STOP
does not implicitly unload resources; explicitly halt/drop as the demo does.
Leaving ReadyBASIC through its normal ReadyOS yield path detaches playback;
returning preserves the cap/tune, but requires explicit MUSPLAY to restart.
Loaders currently read drive 8 using logical file 14, rejecting an already-open
file 14 rather than closing a BASIC-owned channel. Filenames are at most 16
bytes; comma and replacement-prefix syntax are rejected to prevent accidental
DOS write-mode selection. Load/close returns to default KERNAL channels.

## Memory and ownership

- Default BASIC: `$2ac1-$9fff`, unchanged until MEMCAP is called.
- Demo MEMCAP: `$9000`; music owns 4096 bytes, `$9000-$9fff`.
- Always-visible driver: `$9000-$90ff` (second page reserved for ABI growth).
- Certified Summer Vacation payload: `$9200-$9c40`.
- Bridge-owned lifetime byte: `$c1ff` (0 empty, 1 loaded, 2 playing).
- Tick counter: little-endian `$9006/$9007`; init/play pointers `$9008-$900b`.
- Media module: disposable `$b000-$bfff` span, REU code offset `$8000`,
  submodule 24; eight descriptors `$1c00-$1cff` (moved past built-in BORDER
  and USPEED; includes the later graphics commands).
  No ReadyOS shim calls in playback.
- MEMCAP validation uses spare built-in overlay space; only its commit touches
  BASIC's pointers, in the visible core. Cold BASIC capacity isn't the space
  available for adding resident interpreter features.
  Its logical module is SYSTEM (1), physical submodule INPUTEV (19), overlay 2.
  Using proof submodule 5 would collide with disk sample2's cached overlay.

RBR1: eight-byte header `RBR1`, destination (little-endian u16), size (u16),
then exactly size raw bytes. The entire destination must be above the current
BASIC ceiling and end no later than `$a000`; wrap, short and trailing data are
rejected. This is a bounded loader, not a general allocator or VIC-bank mapper.
It refuses to overwrite while a music lifetime lock exists. On I/O failure a
partial resource can remain in the reserved arena, but no music is activated.
Callers own other resource lifetimes and must not raise MEMCAP while using them.
Raw resources are not registered in an allocation table: a later MUSTUNE can
overwrite one placed in the driver's/tune's range. A different resource/tune
combination needs a layout check, even if its PSID header is accepted. The music
demo now calculates colors directly; it no longer loads a palette resource.
RSCFILE remains available for programs that actually need external resources.

## Findings and tensions

1. A SID contains executable code. Header checks cannot sandbox it. This first
   implementation is for vetted/relinked players, not arbitrary HVSC playback.
2. Accepted runtime profile: PSID v2, 124-byte header, PAL VBI, single SID at
   `$d400`, nonzero RTS init/play entries inside loaded data, zero speed mask;
   payload `$9200-$9fff`. RSID, CIA/multispeed, digi and multi-SID are out of
   scope. PAL-only is intentional; NTSC hosts are rejected instead of playing
   the PAL arrangement too fast.
3. GoatTracker code/data relocation succeeded with Sidreloc, constrained to
   `$fc-$fd`, strict pulse comparison and zero pitch tolerance over 100000
   frames. Playback saves/restores `$fb-$fe`, swapping private player workspace.
   This avoids corrupting ReadyBASIC's own scratch pointers.
4. The IRQ cannot point into an overlay slot: any graphics/input/SID command
   can evict it. Only the always-visible reserved driver runs asynchronously.
5. Raster IRQs are separate from KERNAL's CIA keyboard/jiffy work. Servicing
   every raster through the KERNAL jiffy path would change TI/keyboard cadence.
   The driver returns through the ROM register epilogue on raster interrupts
   and chains the original handler for other IRQs. Existing VIC IRQ ownership
   is rejected; the raster comparator is not restored (its previous programmed
   value cannot be read back). On halt VIC interrupts are disabled.
6. Existing under-ROM commands mask interrupts. Long drawing/disk operations
   can drop music frames. CPU acceleration does not by itself make fixed-rate
   calls faster, but cycle-dependent SID writes remain a compatibility risk.
   Ultimate turbo hardware has NOT been validated by these VICE tests.
7. SIDOFF only clears registers; it cannot stop a separate interrupt player.
   Use MUSHALT. Direct sound commands compete for the same three voices.
8. Licensing must cover actual SID/source files, not merely an OGG rendering.
   Shiru provides original GoatTracker songs with explicit CC BY 3.0 permission.
   The BASIC demo contains REM attribution, source/license URLs and modification.
9. The resident core had effectively no slack. Sharing identical warm-entry
   work and removing a one-hop ROM wrapper makes room for the visible commit.
   No BASIC ROM expression evaluator is replaced.
10. Existing KERNAL MEMTOP/MEMBOT constant names had their addresses reversed.
    This implementation corrects them and restores the saved cap on warm entry.
11. MEMCAP is an allocator ceiling, not memory protection. POKE, SYS and
    oversized ordinary LOADs are not sandboxed. Keep the BASIC program itself
    below the ceiling; use the bounded resource loader for external assets.
12. Command names avoid embedded BASIC V2 keywords such as LOAD, STOP and END,
    which the normal C64 cruncher would turn into tokens inside a longer name.

## Verification log

- Offline relocation: pass, exit 0, 0 pitch and 0 pulse-width mismatches.
- First build: resident overflow by 2 bytes; redundant wrapper removed.
- Second build: module branch-distance errors; long branches made explicit.
- Third build: successful normal ReadyOS D81 build; UI probe started.
- First visible UI run: all 51 checks passed (run `vice_auto_20260910_183259`).
- Revised bridge-state UI run: all 68 checks passed, including launcher yield,
  saved cap, stopped-on-return state and explicit music restart
  (`vice_auto_20260910_183905`).
- VIC `$d01a` upper unused bits read as ones: mask with `$0f` before testing
  whether another owner enabled IRQs. Initial playback correctly failed until
  that check was corrected.
- Review found `$c400-$c4ff` doubles as the saved ZP frame. Moved the lifetime
  flag from there to the final byte of the bridge, enforced by a linker assert.
- VICE's host-file PRG autostart did not boot this D81 setup; visible probe now
  types `LOAD "*",8` and RUN, exercising the normal PREBOOT/BOOT/launcher chain.
- Existing regression scripts were run with a local VICE wrapper that places
  `-autostartprgmode 1 -autostart <PREBOOT>` after disk-attachment options.
  No individual app is autostarted. One large-vars boot wait retried; its checks
  subsequently passed.
- Some legacy headless boot waits stalled in the monitor. Read-only register
  diagnostics followed by the monitor's `x` resume let them proceed (large-vars
  and graphics phases 3–5). These are assisted regression runs, not evidence of
  completely unattended harness reliability. No program memory was patched.
- The first eleven aggregate suites passed. The skip-build aggregate then
  reached its launcher-first hotkey test while holding a ReadyBASIC-first disk.
  That setup was interrupted, rebuilt through run.sh with `--run-first launcher`,
  and the hotkey test rerun. Remaining tests use a fresh ReadyBASIC-first build.
- **All 27 regular ReadyBASIC suite targets passed** across the documented
  split runs. Hotkeys passed 161 steps on its launcher-first disk; final
  loaded-app integration passed 222 (`vice_auto_20260910_192248`), and full
  visual verification passed 186 (`vice_auto_20260910_192602`). Final batch
  exited 0, with no degraded steps in these last two integration runs.
  Local logs: `build/readybasic-media-tools/regressions.log`, `hotkeys.log`,
  `regressions-rest.log`, and `regressions-final.log`. Earlier failed audio
  setup remains in the logs for transparency; it was rerun successfully.
- Final plugin/media static checks, Python compilation, release directory
  ordering for regular D81 0.5L, and `git diff --check` passed. Generated test
  plan churn was removed; the user's initially modified loaded-app plan was
  restored byte-for-byte from the pre-build archive. Normal build/version and
  release artifacts remain regenerated. No commit has been made.
- Final visible media run: **85/85 steps passed**, no manual monitor resume
  needed (`vice_auto_20260910_191706`). Confirms exact 4096-byte FRE delta,
  idempotence with heap strings, atomic rejection, IRQ advance/halt, module
  eviction, file-14 ownership, malformed/missing SID rejection, raw resources,
  suspend/resume and sample2/MEMCAP cache separation.
- That run was already executing when WAV finalization was added to the probe.
  Its raw header reported a false 18948-second duration. The capture was
  separately finalized with FFmpeg: **183.210208 seconds**, mono 48 kHz/16-bit,
  sample peak 17857. This confirms non-silent varying output, not a subjective
  listening assessment or a comparison against physical SID hardware. Recording
  includes the later restart checks, not only the BASIC demo.
  Capture: `build/readybasic-media-tools/media-demo-1789093025287717000-final.wav`.
- The first expanded 85-step probe passed its 80 application/boot checks but
  failed five audio-control steps: `monitor.command` needs `raw:` for VICE text
  commands, and the actual command is `resourceset "name" "value"`. Corrected
  the probe rather than changing application code.
- The harness terminates VICE rather than requesting a graceful emulator exit.
  Raw WAVs can therefore keep provisional RIFF lengths. The probe finalizes a
  separate WAV from the actual captured PCM; duration is based on actual sample
  count, not the provisional header. The raw capture is retained locally.

Final binary budgets (regular D81 0.5L): PRG 28674 bytes; resident 6334/6336;
hidden 1959/2048; bridge 512/512; MEMCAP adds 67 bytes in the built-in input-event
overlay. Media SEQ is 1145 bytes, with a 963-byte payload including the 241-byte
resident driver. No additional cold BASIC bytes are consumed.

## Reproduction

```
/bin/bash ./run.sh --profile precog-d81 --run-first readybasic --build-only
python3 build_support/verify_readybasic_plugin.py
python3 build_support/verify_readybasic_media.py
python3 build_support/run_readybasic_media_probe.py
```

The focused probe boots the release disk via PREBOOT, loads/runs the BASIC demo in
VICE's visible UI, captures screens/snapshot and a WAV, and verifies lifecycle
and error paths. It does not require the autostart wrapper. Offline music can
be regenerated with `python3 build_support/prepare_readybasic_music.py` (C
compiler, Python with safe tar extraction, and access to the pinned upstream
Sidreloc archive required). Normal builds need no music download.

Local pre-build generated/user artifacts were archived before rebuilding at
`build/readybasic-media-tools/pre-build-user-artifacts.tgz`; this directory is
ignored so backups and recordings cannot accidentally enter a source commit.

## Physical Ultimate follow-up (2026-09-10)

- Regular D81 0.5L was opened interactively through normal run.sh and left
  available to the user. Physical tests never launch or kill VICE.
- Built Ultimate SKU 0.5M through `run.sh --profile precog-ultimate --run-first
  readybasic --build-only`. Profile inheritance includes RBM.MEDIA, the licensed
  PSID, resource fixture and RBSND07. Release directory-order checks passed.
- Device preflight: C64 Ultimate firmware 3.14 / FPGA 121 / core 1.47, PAL,
  initially 1 MHz. Before-state drive/config captures are kept locally.
- `run_readybasic_media_ultimate.py` runs from Terminal-owned bash. Each run
  builds with the exact destination in `c64u_image_path`, then uploads a
  uniquely named image into its own `USB1/automation/readybasic-media`
  subdirectory, mounts it unlinked on drive A/8, and boots PREBOOT normally.
  It does not overwrite previous images, save firmware settings to flash, or
  touch the user's active VICE process. Turbo tests restore 1 MHz on completion
  (the harness also restores speed on handled failure).
- First physical attempt (`ultimate_auto_20260910_194310`) timed out while
  BOOT was loading LAUNCHER, before ReadyBASIC ran. Existing hardware boot
  scripts document that REST DMA screen polling can stall KERNAL IEC LOAD.
  The physical plan now leaves boot and file-loading intervals quiet before
  inspecting memory. This is a hardware-automation accommodation, not a change
  to the ReadyBASIC implementation.
- Revised run `ultimate_auto_20260910_194750` reached ReadyBASIC, verified the
  original cap, loaded RBSND07, and captured the physical PAL screen showing
  the music playing. The user confirmed it worked and reset the machine during
  the demo. The remaining lifecycle/turbo matrix was then stopped; do not count
  that interrupted run as a full hardware pass or a 16/64 MHz result.
- DMA follow-up: the initial SKU inherited `dma_loading=1` but a blank path,
  so it could only fall back to IEC. Rebuilt 0.5O with an exact image-local path:
  `/usb1/automation/readybasic-media/dma-8cb419d2/rbm8cb419d2.d81`.
  The uppercase spelling used by FTP/mount refers to the same FAT path. Extracted
  `apps.cfg` from the built D81 and checked both the complete path and enabled
  DMA value before upload. The catalog source must be lowercase for the normal
  PETSCII build conversion; an initial mixed-case config was rejected by the
  build validator before packaging. The physical media runner now generates
  this matching config and builds it before every fresh upload, so subsequent
  test images cannot silently revert to blank-path IEC fallback.
- Exact-path acceptance passed **10/10 physical UI steps**, with no degraded
  steps (`ultimate_auto_20260910_195637`). Launcher showed `DMA:YES`, launched
  ReadyBASIC successfully, and showed `DMA:ON` after EXIT, proving DMA was
  actually used. The test returned to ReadyBASIC and left the machine for the
  user; the separate regular-D81 VICE session was not closed or reset.
  Current local image: `Releases/0.5/precog-ultimate/readyos-v0.5o-ultimate.d81`.
  Moving or renaming the deployed image requires updating `c64u_image_path`
  (normally via SETUP); the configured path is specific to this deployment.

## Readable RBSND07 refactor and file-only deployment (2026-09-10)

- Refactored the demo into a short main story and named `PROC`/`EXEC` actions,
  with `FUNC`/`RET%` palette lookup, `FUNC`/`RET` elapsed seconds, and
  `REPEAT`/`UNTIL` animation/wait loops. Timing uses TI rather than CPU-dependent
  iteration counts. The elapsed-time function accounts for TI's midnight wrap;
  that boundary case was not exercised on hardware in this run.
- Preserve the original border color. Halt retains the song, replay restarts
  it, and final cleanup drops the driver before `CLR` and `MEMCAP(40960)`.
  Keep CLR outside active procedures and before raising the memory ceiling.
  Attribution and the offline relocation note remain in REM lines.
- No full build: tokenized only the demo using the existing build recipe:
  `petcat -w2 -l 2ac1 -o obj/rbsnd07_psid.prg -- src/apps/readybasic/rbsnd07_psid.bas`.
  Checked all 76 linked BASIC lines and their source line numbers; the 1989-byte
  PRG ends at $3284, safely below the reserved $9000 ceiling.
- Ultimate FTP supports entering the mounted D81 as a virtual directory and
  replacing `rbsnd07.prg` directly. LIST sizes are block-rounded, while RETR
  returns exact bytes. Backed up the prior PRG, replaced only that entry, and
  verified readback against the local PRG. Directory names and `apps.cfg.seq`,
  `rbm.media.seq`, `rb.summer.seq`, and `rb.colors.seq` remained unchanged.
  Original backup: `build/readybasic-media-tools/rbsnd07-update-c83c4d9b/rbsnd07-before.prg`.
  Deployed PRG SHA256:
  `b6143a06ba63608362d278e301fcfafe559d1458aebf77d746de1fd8accba46e`.
- Physical UI acceptance passed **22/22 steps**, no degraded steps:
  `logs/ultimate_auto_20260910_201713/manifest.json`. Ran the demo twice without
  resetting, saw all lifecycle messages and no BASIC errors, and checked both
  the cleared media flag at $C1FF and restored MEMSIZ=$A000 after each run.
  This validates the nested function expressions in the animation and UNTIL
  condition on the existing interpreter. It is not a new audio capture or a
  turbo-speed acceptance result.
- Left the updated program loaded with the animation procedure listed. The
  user's separate VICE session was untouched. Local source and obj PRG are
  updated for future builds; the local release D81 was deliberately not rebuilt
  or patched, so its old RBSND07 differs from the deployed image until a build.

## Native BORDER integration (2026-09-10)

- Added `BORDER(C)` to built-in GFXCORE (logical graphics module 3, slot 1),
  command ID 115; IDs 110-114 remain owned by media. Uses the existing numeric
  signature and masks to the low four bits, matching MCBG. It affects only the
  VIC border, independently of the selected graphics mode.
- The handler is 11 bytes. GFXCORE's shared payload is now 1356/2048 bytes,
  leaving 692 runtime bytes; the tighter CMDPACK seed budget has 139 bytes free.
  The 32-byte descriptor consumes a filler entry, not extra resident memory.
  BASIC starts at $2AC1 with 30013 empty bytes; cold PRG remains 28674 bytes.
- Correction to the initial estimate: spelling BORDER needs command-reader
  support because BASIC V2 tokenizes its embedded OR as $B0. Extended the
  existing FN/FRE/PI name expansion with OR, sharing the two-letter cases.
  Resident code shrank by 8 bytes to 6326/6336. The crunch hook, stored token
  format, numeric assignment evaluator and BASIC ROM OR operator are unchanged.
- Tests caught a separate registration collision: the initial BORDER entry at
  $1BC0 was replaced when loading the older media package. This caused syntax
  errors on both `BORDER(shade(p%))` and a plain-variable BORDER call after
  loading media; it was not evidence of a native-function argument limitation.
  Moved media descriptors to $1BE0-$1C7F, rebuilt the package and added a static
  assertion that its destination consists entirely of free built-in entries.
  Do not combine this interpreter with the earlier experimental media package.
- RBSND07 retains `BORDER(shade(p%))` and `BORDER(bc)` for animation and restore.
  Its PEEK of the original border and palette bytes is intentional; it no longer
  POKEs the border register. Shiru's attribution remains in the BASIC REM lines.
- Focused visible VICE run passed **98/98**, no degraded steps:
  `logs/vice_auto_20260910_205512/manifest.json`. Includes ordinary OR and integer
  REPEAT/UNTIL, direct BORDER, low-nibble masking, the nested FUNC demo,
  original-border restoration, media eviction and ReadyOS suspend/resume.
  Finalized audio: `media-demo-1789098911958227000-final.wav`, 200.45 seconds,
  peak 12727 (capture includes file loading and harness time).
- Rebuilt regular D81 0.5T and Ultimate D81 0.5U through normal run.sh. Both
  release-directory ordering checks passed. Extracted the Ultimate interpreter,
  media package and demo and compared each byte-for-byte with its built artifact.
- Uploaded 0.5U into a new owned folder without overwriting previous images:
  `/USB1/automation/readybasic-media/border-fc94ff4f/RBfc94ff4f.D81`.
  Verified the full FTP readback; SHA256
  `faebf8dba90c55f1d87c2a46c5b2652728083c1920fb05fb59618b87f14fb856`.
  Embedded apps.cfg has DMA enabled and the exact image path (lowercase PETSCII
  source spelling), with runappfirst empty. Reset and issued the normal
  `LOAD "*",8,1` / `RUN` disk boot, not a standalone app launch.
- Left physical hardware at **READY OS V0.5U / DMA:YES**, captured once boot
  completed (`build/readybasic-media-tools/border-launcher-final.txt`). No
  hardware demo/test sequence was run, per the user's request. A screen read
  79 seconds after RUN still showed the loader; after a quiet interval it
  reached the launcher. The user's original VICE process was left untouched.

## Integer-only border demo (2026-09-10, supersedes palette demo above)

- Removed the generated `rb.colors` palette, its profile payload and the demo's
  RSCFILE call. The retired filename is still treated as build-owned so disk
  preservation cannot silently bring it back. Generic RSCFILE remains in media.
- SHADE takes the previous integer color and returns `(ix%+1) AND 15`.
  ANIMATE assigns that result to C%, calls `BORDER(C%)`, and waits a quarter
  second using the elapsed-time function. No palette PEEKs or nested color/time
  expression are needed. Song attribution and the playback lifecycle remain.
- Important correction to earlier verification: completion, audio, and border
  restoration did not prove visible animation. A first no-palette version using
  `shade(int(age(mk)*4))` finished but sampled color 0 eight times. This does not
  establish the underlying expression-runtime cause; the simpler explicit
  counter is proven below. No interpreter changes were made for this revision.
- Added `READYBASIC_DEMO_ONLY=1` to the media probe. It boots ReadyOS normally,
  runs the actual disk demo, records SID output, samples D020/C1FF/music ticks,
  checks function wraparound, and verifies final memory/border restoration.
  A constant border now fails the probe even if every harness step succeeds.
- Focused VICE run: **45/45 passed**, no degraded steps, followed by animation
  and audio assertions. `logs/vice_auto_20260910_214559/manifest.json`.
  Border samples: **1, 2, 3, 3, 4, 5, 6, 6**; music ticks:
  **34, 60, 81, 102, 123, 149, 170, 191**; playing state remained 2.
  SHADE(0)/SHADE(1)/SHADE(15) returned 1/2/0. Final border 6, media state 0,
  MEMSIZ $A000, and no BASIC error. Audio: 37.72 seconds, peak 13095,
  `media-demo-1789101958885311000-final.wav` (includes load/harness time).
- Regular release 0.5W passed directory ordering; RB.SUMMER, RBM.MEDIA and
  RBSND07 are present, RB.COLORS is absent. The retired 24-byte generated host
  fixture was moved into ignored build scratch storage, so it is recoverable.
- Per the user's instruction, no further full regression run was started.
  The previously running loaded-app suite had already ended successfully
  (223 steps, `logs/vice_auto_20260910_213526/manifest.json`). Its two long-demo
  timing waits and the cross-app app-banner wait are retained as harness fixes.
- Ultimate release **0.5X** passed directory ordering and has no RB.COLORS.
  Extracted RBSND07 (2009 bytes), RBM.MEDIA (1145), RB.SUMMER (2751), and
  READYBASIC (28674) all matched their built/source artifacts byte-for-byte.
  Uploaded into a new owned folder, preserving the earlier hardware image:
  `/USB1/automation/readybasic-media/border-d5ac0d39/RBd5ac0d39.D81`.
  Full FTP readback matched SHA256
  `2ca2f8e0b1cef42ced3f8f0ef2328471e2bbd45e847a86bc19129f9bda7f8aaa`.
  Embedded apps.cfg: DMA_LOADING=1, empty RUNAPPFIRST, C64U_IMAGE_PATH exactly
  matching that image (PETSCII source spelling). Issued the normal disk boot;
  no physical music-demo or regression automation was sent.
- After a quiet boot interval, a one-off screen read confirmed the physical
  launcher at **READY OS V0.5X / PRECOG ULTIMATE / DMA:YES**, with Browse and
  Load selected (`build/readybasic-media-tools/integer-step-launcher.txt`).
  ReadyOS is left there for manual testing; the earlier VICE session is intact.

## 2026-09-11 — streamed graphics loading with visible sprites

- The user reproduced the incomplete AD/partial-color startup on physical C64U
  after an earlier passing run. One attractive capture was not repeatability
  evidence; the early `ORBITAL SHOW RUNNING` marker is not load completion.
- All media-file loaders now suppress sprite DMA throughout KERNAL I/O and
  restore the saved mask after CLOSE, including error paths. IRQ masking alone
  does not prevent VIC sprite cycle stealing. MCFILE also hides the bitmap
  until its pixels, palettes and exact EOF have arrived. No extra BASIC or
  unowned REU staging memory is used; failed reads are not full-image rollback.
- Final untrapped true-drive VICE: 100/100 steps plus exact-byte/animation audit,
  `logs/vice_auto_20260911_142659/manifest.json`. Missing files and wrong headers
  restore sprite/display state and leave no logical file open.
- Final cold hardware load and live 1/16/64 MHz checks passed at
  `logs/ultimate_auto_20260911_142855/manifest.json`. The user also independently
  confirmed the earlier sprite-guard build was visibly working on the C64U.
- Parallel probes exposed ambiguous manifest selection in the test wrapper.
  Plans now have unique IDs/paths; the separate exact-run auditor consumes the
  saved compiled plan and its own bitmap paths. The correct final run was
  audited successfully after fixing the selector.
- Full research, limits, artifact hashes and final paths are recorded in
  [the load investigation](readybasic_media_load_investigation.md). An idle or
  disconnected IEC peer can still trap a stock KERNAL clock wait; this change
  does not claim a universal bus watchdog or install an invasive NMI handler.
- Stronger repeat test with Manual 16 MHz active before RUN failed at MCFILE
  (`logs/ultimate_auto_20260911_143229/manifest.json`). Loading at 1 MHz and
  playback at 16/64 MHz are different claims. Manual turbo disables software
  speed registers, so do not promise automatic safe I/O in that configuration.
  The manual handoff is restored to 1 MHz so repeating RUN stays supported.

## Two demo variants and software turbo (2026-09-11)

- `USPEED(mhz)` belongs in a built-in overlay for now: putting the only way to
  downshift inside a disk-loaded module would require the unsafe load first.
  It uses 75 additional INPUTEV bytes (251/2048 total), no new resident parser
  or BASIC memory. Media registration moves to $1C00 after its descriptor.
- Use the documented C64 Ultimate / Elite-II D031 table, not the older U64
  table: 16 MHz is index 9 here. Preserve bit 7. Manual/Off expose $FF and are
  rejected; invalid MHz values are rejected before hardware access.
- New `RBGFXSNDDEMO` and `RBUGFXSNDDEMO` replace the old disk demo entry to
  keep the Ultimate D81 within capacity. The old source is retained.
  This SKU now has only one free disk block: future sample/resource additions
  need deliberate disk-content budgeting, not silent truncated writes.
- Precalculate sprite heights and line endpoints before showing graphics;
  use five unrolled SPRMOVE calls and an integer-result PHASE function. A
  roughly 2.13-second wave is clock-driven, not multiplied by CPU speed.
  This is still BASIC animation, not a raster-synchronized sprite engine.
- Q releases graphics and music, then CLR/MEMCAP. M releases graphics and
  BASIC arrays but preserves the music arena at $9000. Both restore the saved
  border/background/foreground. Ultimate also returns to 1 MHz on both exits.
- The first prototype exposed two unsupported BASIC forms: scalar integer
  assignment of -1 reached GETADR and failed, and a zero-argument FUNC call
  produced syntax error. Use sentinel 256 and `FUNC PHASE(tk%)` with
  `RET%`, matching the existing RBSND07 integer-result pattern. These are
  scoped demo corrections, not interpreter fixes.
- `EXEC WEAVE:LT=TI` failed on return at its caller line; giving the EXEC
  and assignment their own lines made the animation run. Keep this as a
  demonstrated narrow workaround, not a blanket claim about every inline EXEC.
- Test the hidden text screen for errors as well as SID state: an IRQ tune
  and intact bitmap can keep running after BASIC has already stopped.
- Physical test discipline: REST memory reads (including decoded-screen and
  memory-assert steps) can lock up C64U disk I/O. A 110-second startup wait
  proved insufficient for this heavier precomputation/load sequence; that test
  encountered I/O error 21 with the speed register still at 1 MHz. Do not treat
  it as evidence that software-selected 1 MHz is unreliable. Subsequent tests
  use video-stream-only observation and require moving cyan READY letters
  before enabling memory inspection. Failed interrupted transfers get a fresh
  ReadyOS boot, not an assumption that retrying RUN repairs the bus state.
- Blanking is not staging: MCFILE currently writes final bitmap/palette RAM
  with DEN clear. Its error cleanup restores the saved display settings and
  can therefore reveal partial data. There is no full-image rollback guarantee.
- Final physical proof used video-only startup observation: all three RUNs
  reached visible animation in about 118 seconds, including the rerun after M.
  Successful inspection blocks: `ultimate_auto_20260911_154905` (native
  1/16/64 MHz), `155215` (scene, refresh, M), `155457` (rerun, Q), and `155722`
  (final running handoff), under `logs/`. Both exits restored test colors
  4/6/14 and speed index 0; M retained an advancing music IRQ at the prompt,
  Q detached it and restored MEMSIZ=$A000. Final handoff saved normal 6/6/1
  text colors and left the demo running at 16 MHz.
- VICE two-exit proof: 41/41 steps plus byte/movement/refresh checks,
  `logs/vice_auto_20260911_151833/manifest.json`. Existing media regression:
  122/122 steps, `logs/vice_auto_20260911_155241/manifest.json`; separate audit
  confirmed changing border colors, advancing IRQ ticks and 155.73 seconds
  of nonconstant audio (peak 12745). The wrapper initially miscounted the
  latest-run symlink as a second run; excluding that alias fixed selection,
  and the actual successful run was audited without rerunning hardware.
- Broader regression is partial, not a full-suite pass: 11 core targets passed
  (resume_min passed on retry after a startup timeout). The aggregate's hotkey
  fixture requires launcher-first boot, incompatible with the current
  ReadyBASIC-first demo image under skip-build. That target was stopped.
- Final packaged regular 0.5D and Ultimate 0.5E D81s passed exact-byte runtime,
  module, demo/resource and directory-order checks. Ultimate upload/readback:
  `/USB1/automation/readybasic-media/neon-5807f3ef/RB5807f3ef.D81`, SHA-256
  `bd6a80b606762bf1d820620802ea29857fac7cf9bcb95567b011cb03cf3a0d94`.
  Embedded apps.cfg matches that path, with DMA_LOADING=1 and
  RUNAPPFIRST=READYBASIC. Retired the old generated fixed-wait demo test;
  preparation now emits only the attach configuration for the gated runner.
- Follow-up test-tool hardening, locally syntax-checked without disturbing the
  final handoff: boot and the initial BASIC program LOAD now require explicit
  operator confirmation of the physical idle prompt before REST inspection.
  The successful hardware run above predated these two operator gates; its
  three resource-loading RUNs already used the verified video-only gate.
