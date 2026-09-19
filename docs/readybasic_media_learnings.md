# ReadyBASIC MEMCAP / media experiment

Current 0.5 RC2 command packaging: use `LDMOD` and `PAUSE`. There are 83 built-ins;
media descriptors use `$1A40-$1B3F`, and disk demo packages use a separate
`$1B40-$1F3F` area. Sample3 replaces other demo entries, preserving production
and media commands. Older offsets/counts in dated evidence below describe
those earlier builds. See the [sample module guide](../src/apps/readybasic/READYBASIC_SAMPLE_MODULES.md).

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

`LDMOD("rbm.media",M%)` now loads eight commands, without consuming BASIC RAM.
The original five are listed here; MCFILE, SPRFILE and MCLINE were added later
and are detailed in the [current command/reference guide](readybasic_reference.md)
and [Orbital Echoes walkthrough](readybasic_orbital_echoes.md):

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
  submodule 24; eight descriptors `$1c20-$1d1f` (moved past built-in BORDER,
  USPEED and UMHZ; includes the later graphics commands).
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

## Space-triggered background restore (2026-09-11)

- Both current demo sources now dispatch Space to the existing CLEAN procedure:
  restore the owned REU surface, reset the automatic-refresh clock, and count
  the refresh. No disk I/O, new module code, or change to Q/M cleanup.
- Regular 0.5F and Ultimate 0.5G images rebuilt through run.sh. Both packaged
  demos match their compiled sources; directory ordering and Ultimate DMA path
  checks passed. These builds have not replaced the image on the physical C64U.
- Focused VICE artifacts: `logs/vice_auto_20260911_160624/`. Around Space,
  RC changed from 0 to 2: manual restore plus an automatic expiry in the same
  short observation window. Music remained active; palette/movement, M music
  retention and colors, rerun, Q music release and MEMSIZ restoration passed.
  The final text-color query did not complete; the exit screenshot shows stray
  Q input in the editor. This is not recorded as a completely green suite.
- The probe now allows the legitimate automatic/manual coincidence and clears
  stray exit-key input before its final diagnostic. That final input-cleanup
  adjustment is syntax-checked, not claimed as a fresh full VICE run.

## Live Ultimate speed query (2026-09-11)

- The reported 1 MHz observation did not reproduce as a failed USPEED write:
  the original deployed setter ran a 1,000-iteration BASIC loop in 89 jiffies
  at 1 MHz, 5 at 16 MHz, and 2 at 64 MHz. CPU PEEK read D031 indexes 0/9/15.
  Do not confuse the Ultimate settings preference with the CPU's live register.
  The demo's sine period intentionally follows the clock, not CPU throughput.
- Added `UMHZ()` using the existing no-argument integer-result signature:
  `PRINT UMHZ()` and `S%=UMHZ()` read the nominal live MHz. Unavailable software
  registers (Off/Manual) return error 24. It also honors the D030 enable gate
  when that gate is exposed, rather than reporting an inhibited D031 speed.
  This is a setting readout, not an effective-cycle measurement.
- USPEED now verifies its written D031 index. Getter plus verification costs
  65 additional INPUTEV bytes: 316/2048, no resident/BASIC workspace growth,
  and the cold PRG remains 28674 bytes. UMHZ is command 120 / descriptor $1C00;
  the matching media descriptors move to $1C20-$1D1F. Never mix older packages.
- Physical-only regression, per user request (no VICE):
  `build_support/run_readybasic_uspeed_ultimate.py --idle-confirmed`, executed
  in a Terminal-owned shell. All 32 checks passed at
  `build/readybasic-media-tools/uspeed-1789172938346762000/results.json`:
  all 16 supported MHz values, integer assignment/expression, invalid argument
  nonmutation, badline-bit preservation, normal integer REPEAT, timed loops,
  Off/Manual query/setter errors, and final 1 MHz. New-build loop samples were
  89/6/2 jiffies at 1/16/64 MHz. The saved `config-at-live-64.json` still says
  CPU Speed 1 while the following UMHZ assignment returns 64.
- Ultimate 0.5I exact-path build/upload/readback:
  `/USB1/automation/readybasic-media/neon-308f649a/RB308f649a.D81`, SHA-256
  `89393ea56d35b360473751eaaa4e7cfe568016ff2803cb44c891a01cf19a52f4`.
  Embedded apps.cfg matches that path with DMA_LOADING=1. Runtime, media and
  demo bytes were extracted and compared; static ABI and directory order passed.
- The loaded Ultimate demo also passed a live integration probe: a temporary
  RAM-only `385 MS%=UMHZ()` line sampled the getter after graphics/music module
  activity. Video-only readiness passed at 116.1 seconds, then three samples
  returned 16 MHz while music ticks advanced 119/173/227. Space advanced RC
  from 0 to 1 without stopping playback. Evidence:
  `build/readybasic-media-tools/umhz-demo-proof.json`, `umhz-demo-run.log`, and
  `umhz-demo-video/motion-ready.png`. Q released music/MEMSIZ and UMHZ returned
  1 at the prompt. The temporary line was removed, then `USPEED(16)` / UMHZ
  showed 16 at the idle handoff (`video-only/173416.png`). The demo is loaded;
  RUN downshifts for resource loading. Use USPEED(1) before other disk loads.
- A second CC0 image, Ansimuz's Warped City, is prepared separately under
  `assets/readybasic/warped-city/`. Not in the demo or disk. Another image needs
  40 disk blocks, so later integration must resolve disk capacity first.

## Turbo mechanism versus line-pattern regression (2026-09-11)

- Independent host-clock comparison of the same 10,000-iteration stock BASIC
  loop: D031 1 MHz 11.3827 s; D031 16 MHz with menu preference 1, 0.8422 s;
  Manual 16 MHz 0.7338 s; Gouraud-style D030 enabled/menu 16, 0.7209 s;
  D030 disabled, 11.2422 s. REST overhead precludes ranking the fast modes from
  these small differences. Both paths accelerate real work.
- Actual pre-fix demo counters: 16/1/16 MHz gave 12.32/0.75/12.44 completed
  five-sprite batches/s and 11.33/0.62/11.08 lines/s. The wave's period stayed
  clock-driven. A CPU benchmark therefore did not establish good animation.
  `graphics-speed-1789174360205122000/results.json` is the complete repeat;
  the first attempt stopped because its fixed two-second speed-key wait was
  too short at 1 MHz, not because a speed write had failed. The probe now
  waits for the BASIC-side acknowledgement before measuring.
- The original RBSND08 line phase had an eight-second cycle and both mirrored
  lines shared the same sample. The later sprite speed-up reused a 2.13-second
  phase for lines and alternated mirror sides on separate samples. This is a
  concrete geometry regression: much larger gaps, even with working turbo.
  Both current demos now use an independent LP% and increment one sample only
  after the mirrored pair completes; sprite phase remains time-driven.
- Ultimate preparation now selects 64 MHz for DIM and precalculation, then
  1 MHz before LDMOD/scene/MUSTUNE, then 16 MHz for playback. Both exits still
  restore 1 MHz. This does not add a general ReadyOS speed/IRQ guard.
- Gouraud's source explicitly justifies D030 for 1 MHz KERNAL keyboard scans
  and return to a configured 64 MHz. No evidence was found that it was chosen
  as a universally superior speed mechanism. The recommendation and sources
  are in [the speed-policy note](readyos_ultimate_speed_policy.md).
- Follow-up to the earlier handoff entry: an optional full-program REST RAM
  comparison timed out after the successful Q/query test. It is not a passed
  byte-identity test. Later video showed stock BASIC; the cause of that reset
  was not established. A fresh mounted-image ReadyOS boot was used for the
  graphics comparison; no memory reads were made during boot or disk loading.
- Corrected physical run:
  `build_support/benchmark_readybasic_demo_ultimate.py --loaded-idle-confirmed
  --verify-setup`, Terminal-owned, evidence under
  `build/readybasic-media-tools/graphics-speed-1789174818141818000/`.
  Video readiness: 37.6 s versus the earlier 116.5 s. CPU-side setup samples
  confirmed PS%=64 before precalculation and LS%=1 before disk/module loading;
  playback samples confirmed 16/1/16. Completed sprite batches/s were
  13.31/0.62/13.07, with 6.84/0.75/7.59 lines/s. LP% matched the paired-line
  counter throughout (allowing a sample between return and counter increment).
  The sparse phase jumps became closely spaced mirrored fans in real video.
  Automatic refresh advanced; Space restored the image while music ticks
  continued. Q detached music and returned MEMSIZ=$A000. Temporary test lines
  were removed without saving them to disk. These timings include test overhead;
  this is not a claim of 50/60-fps sprite animation or a new audio-quality test.
- Ultimate 0.5J was built through run.sh, exact-byte checked for the runtime,
  module, both demos and resources, and passed release directory ordering.
  Uploaded/read-back SHA-256:
  `09fa313ce5cbd9c29f703c041d13118d85fb589a9cd4859669d1fca4a84cc933`.
  Path: `/USB1/automation/readybasic-media/neon-0f53bc00/RB0f53bc00.D81`;
  embedded apps.cfg names this exact path with DMA_LOADING=1. The standard
  demo source is updated for future builds; no regular-SKU rebuild or VICE run
  was performed for this correction.

## Raw-motion follow-up (2026-09-11)

- User still found the clock-driven sprite movement jerky and explicitly
  requested simple raw iteration rates. Both demo sources now use SP%=2
  samples per sprite batch and LD%=3 batches per line, with consecutive paired
  LP% line samples. Removed the PHASE function, phase wait and clock-derived
  motion. TI is used only for background renewal. This also removes a hot-path
  FUNC lookup, without changing any interpreter code.
- The counter probe now checks sprite phase against completed batches, lines
  against the three-batch cadence, and LP% against completed line pairs.
- Ultimate 0.5K: local build, exact packaged bytes, static checks and directory
  order passed. Uploaded/read back to
  `/USB1/automation/readybasic-media/neon-dd2847ed/RBdd2847ed.D81`, SHA-256
  `f0fa8b6fc2229e72c7c8a0f52a699a925c7f79374428ff41f720a57c89e170ed`.
  Embedded apps.cfg matches the path. The upload succeeded, but the subsequent
  REST configuration request timed out before mount/reset. This raw-motion
  image was not yet physically tested at that point; the 0.5J tests above cover
  the intermediate clock-paced sprite version only.
- User confirmed the menu was closed and the intermediate demo was playing.
  Subsequent Terminal-owned HTTP and FTP-port probes both timed out, as did
  ping, while the Mac retained 10.0.0.15/en0 and the target remained configured
  as 10.0.0.79. Therefore do not attribute this interruption to the menu or
  claim a demo crash. Restoring network reachability was required before
  mounting and testing 0.5K. No REST RAM reads occurred during the failed
  deployment, and no new-image boot was started.
- After the user's restart, the API returned and 0.5K was mounted and booted
  through normal ReadyOS. Raw-motion physical proof passed under
  `build/readybasic-media-tools/graphics-speed-1789175697249434000/` using
  `benchmark_readybasic_demo_ultimate.py --loaded-idle-confirmed --verify-setup`.
  Video-only readiness took 37.7 s. PS%=64 and LS%=1 verified setup stages;
  the playback comparison verified 16/1/16 MHz. Sprite batches/s were
  29.37/1.74/29.73; lines/s were 9.83/0.62/9.83. Counts confirmed two raw sprite
  samples per pass, one line per three passes, and one LP% sample per mirrored
  pair (with allowance for snapshots between the operation and its counter).
  No motion phase depended on TI. These are instrumented throughput samples,
  not a claim of frame-synchronized display updates.
- Automatic refresh, Space restore with advancing music ticks, and Q's music
  release/MEMSIZ=$A000 passed. Test lines were deleted without saving to disk;
  streamed video showed an idle prompt and UMHZ=1. The normal uninstrumented
  RUN then reached video-confirmed animation at 37.9 s and was left playing
  for the manual handoff (`raw-final-run.log`, `video-only/181824.png`).
  No VICE was used in this round.

## Two cached pictures, 64 MHz playback, explicit M regression (2026-09-11)

- Both sources now preload the CC0 Ansimuz Warped City image as well as space,
  using distinct GFXSURF handles. Loading stays at 1 MHz before MUSTUNE/MUSPLAY;
  precalculation and Ultimate playback use 64 MHz. The automatic 900-jiffy
  refresh toggles the image; Space restores the current one and restarts the
  timer. Both pictures use D021=0. There are no disk reads in the live effect.
- Line tables now have 512 phases, halving the angular increment per mirrored
  pair. Sprite motion remains raw SP%=2 / LD%=3; no clock-driven phase or
  catch-up was reintroduced. The larger arrays still fit beneath MEMCAP($9000).
- SND08 was already absent from the release directory. Appending another
  40-block Koala would not fit. Added backward-compatible RKC1 decoding to
  MCFILE in rbm.media and a lossless offline packer; canonical Koala files are
  unchanged. Packed pictures total 9,141 bytes versus the former single 10,003
  bytes. After module/demo growth, Ultimate 0.5L still has one free disk block.
  No other examples/apps were removed. Payload is 2,106/4,096 bytes in the same
  two module slots; package 2,384 bytes, driver still 241 bytes. No interpreter,
  resident workspace, descriptors or music relocation changes.
- The built 6502 decoder passed 20 py65 cases through a simulated KERNAL:
  both canonical Koalas and packed images, repeats/literals, bad magic, short
  packets, extra data, oversized expansion, and RBR1 sprite compatibility.
  Guard regions and display/sprite restoration were checked. This proves the
  decoder's data/bounds behavior, not physical IEC timing. The physical demo
  below subsequently exercised both packed reads twice at 1 MHz.
- M investigation: the initial existing machine was already at a BASIC prompt
  hidden behind the bitmap; its hidden text contained a LIST. Injecting M then
  merely typed M at that prompt. That observation is not evidence of a fresh
  M exit and does not establish the original cause. Cleanup now restores text
  and colors before releasing image handles; both exits use the same FINISH.
  The new fresh-run regression explicitly verifies M rather than inferring
  it from Q or from the fact that music continues.
- Physical proof: Terminal-owned
  `benchmark_readybasic_demo_ultimate.py --loaded-idle-confirmed --verify-setup`,
  evidence `build/readybasic-media-tools/graphics-speed-1789177193927298000/`.
  First video-only readiness 39.2 seconds; music-active rerun 40.5 seconds.
  PS%=64 / LS%=1 confirmed setup speeds. Playback 64/1/64 measured sprite
  batches/s 90.91/1.74/91.15 and lines/s 30.34/0.62/30.34. Raw sprite and line
  phase/cadence assertions passed throughout. These are instrumented throughput
  measurements, not a claim of frame-synchronized or tear-free sprite output.
- Full screen/color palettes matched each canonical source on city→space→city
  restores, with observed polling intervals 14.6 and 16.1 seconds around the
  15-second timer. Space restored the image while SID ticks advanced. M left
  music state 2 with advancing ticks and MEMSIZ=$9000, but cleared bitmap and
  multicolor bits, disabled all sprites, and restored border/background/text
  colors 6/6/1. Rerunning with music still active passed the video-only loading
  gate. Q then detached music, restored MEMSIZ=$A000, and passed the same text,
  sprite and color assertions. All temporary probe lines were deleted without
  saving to disk. Audio quality was not measured in this run.
- Built through normal run.sh, exact disk payload comparison and release
  directory-order checks passed for Ultimate 0.5L. Uploaded and read back:
  `/USB1/automation/readybasic-media/neon-d91a3778/RBd91a3778.D81`, SHA-256
  `e627b059cf57e32c1a5e7dd815a17390262527ad837677c3965517cf43566f6a`.
  Embedded apps.cfg matches that path with DMA_LOADING=1. Boot used normal
  PREBOOT/BOOT/launcher; only video was observed until loading completed.
  Standard source/profile updated for future builds; no standard rebuild or
  VICE run was performed in this Ultimate-only round.
- The final in-RAM program was compared byte-for-byte with the shipped PRG
  after removing instrumentation. A normal uninstrumented RUN reached
  video-confirmed animation in 40.3 seconds and was left playing at 64 MHz.
  Evidence: `two-final-run.log`, `video-only/184419.png`. The older two-exit
  runner audits now accept either complete matching scene palette; those
  older runners were syntax-checked, not rerun here. The expanded physical
  benchmark above is the current proof of actual alternation and both exits.
