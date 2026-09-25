# Experiment progress — 2026-09-25

The initial goal turn made progress: branch, verified backup, real native C64
OS partition test, normal ReadyOS boot with IDE64, and a working native
whole-RAM transfer gate. The full two-OS objective remains active and incomplete.

## Authoritative working state

- Branch `experiment/c64os-readyos-bridge` from `f505038`.
- Private environment `../c64os-readyos-experiment`, baseline and runtime copies
  plus SHA-256 manifest. Original `../c64os` was not edited.
- The runtime HDD has a deliberately local 32-bank capacity cap in rec.lib.o.
  Installation/readback evidence: `reu-cap-install.json`.
- Latest regular D81 at this checkpoint: `readyos-v0.5c-d81.d81`, with skip 39,
  built by `/bin/bash experiments/c64os/run.sh --build-only`.
- Running ReadyOS test process was launched through the normal run.sh wrapper;
  binary/text monitors 6511/6611. Inspect its live state before using it again.
  The standalone C64 OS preparation process on 6510/6610 has exited.
- Generated release, bin and version-file changes in the checkout are from
  the normal build flow. Pre-existing untracked Ultimate releases and uZIP
  build output were present before this work; do not remove them.

## Tests and discoveries

1. Native C64 OS boot baseline via copied `run-vice.sh ide64`, correct copied
   `idedos20190819-c64.crt`, 16 MB REU. Desktop screenshot + VSF recorded.
2. `python3 experiments/c64os/cap_native_reu.py`: native LOAD matches known
   detector, SAVE changes only the copied HDD, reload matches patched code.
3. CPU-executed `reu_canary.s` followed by native C64 OS LOAD/RUN: 448 pages
   outside banks 0–31 retained their patterns. Full screenshot/VSF evidence.
   The earlier monitor-only seed/fetch attempt did not execute DMA and is invalid.
4. ReadyOS build + boot via wrapper: PRG autostart failed under IDEDOS; replaced
   in the wrapper by native D81 LOAD"*",8/RUN. Fast traps returned device not
   present; true 1581 emulation worked. Launcher visible, live shim skip=$27.
5. `python3 experiments/c64os/verify_bridge_ui.py`: three destructive transfer
   cycles pass, entire shim compared, successful return to launcher. Proof
   comes from the real launcher app and keyboard-driven UI, not injected app
   execution or simulator snapshot restoration.
6. `python3 build_support/verify_release_directory_order.py --profile precog-d81`:
   PASS on v0.5c, PREBOOT first, experimental app in normal program group.
7. `git diff --check`: PASS.

## Next implementation work

The app currently exposes only the S transfer proof. Do not describe it as a
working C64 OS switcher. Extend it to cold-load C64 OS and warm-resume its saved
image. Build/install a native 1.09 utility for the reverse direction. Preserve
all of the original requested state, including interrupts, ZP, stack and shim.

The trampoline at $0400 uses a delayed 64 KB REU transfer with I/O mapped out.
Both images must contain identical trampoline instructions when fetched. It
resumes via a pointer and saved stack pointer at $0702/$0704 into native code,
which then replaces the displaced $0400–$07FF bytes and color RAM. `$FF00` is
explicitly preserved because it triggers delayed DMA. The proof intentionally
destroys ZP, stack and shim between save and fetch; remove that destructive
step from normal cross-OS operation while retaining it as a diagnostic gate.

The current wrapper does not capture CIA/VIC/SID state or arbitrate disk/network
channels, and it is not sufficient for switching OSes yet. CIA masks/timer
latches and SID registers cannot be recovered by a naive I/O read. Baseline
C64 OS uses VIC IRQ with CIA1/CIA2 masks zero and timers stopped. Investigate
IDE64 cartridge RAM/file-channel state as well as C64 main RAM.

The online utility vector documentation is stale relative to installed 1.09
headers. Verify the actual HDD utility ABI; the loose header has init, message,
quit, freeze, thaw, identity (six words). Also solve utility reopening after
resume through normal C64 OS lifecycle, rather than leaving a permanently
registered invisible utility that cannot be invoked twice.

## Second implementation checkpoint (2026-09-25)

- Added common native_context.inc for ReadyOS and a real six-vector C64 OS
  utility below the util.frame boundary $F71C (current BSS ends around $F020).
  It captures all main RAM, original $0400-$08FF, color RAM, readable VIC/CIA
  state, interrupt vectors, REU registers, CPU ports/registers/stack. The core
  also snapshots 28KB IDE64 DOS RAM through OPEN + CPU staging at $0800, to
  banks 33/35; main RAM is banks 32/34. This uses CPU-executed DMA.
- Version-2 record is in control bank 39 at $FD40: ASCII RBG2, byte 4 = 2,
  byte 5 = ReadyOS valid, byte 6 = C64 OS valid, byte 7 reserved. A context is
  marked valid only after both its main and IDE64 RAM copies complete.
- CIA write-only mask/reload and VIC compare values use explicit supported
  quiet-text-mode contracts; SID is silenced, not faithfully restored. General
  active SID playback / arbitrary IRQ drivers remain outside the proven scope.
- Cold loader at $CF00 recreates KERNAL/BASIC low workspace while retaining
  IDEDOS I/O vectors, sends CP1 + CD//OS, LOADs BOOTER then jumps to $080D.
  Failure has a trampoline return path, but has NOT yet been negatively tested
  or given a distinct error message.
- Native utility defers handoff via a one-shot normal timer. After warm resume,
  it calls util.frame uf_kill ($FF1B), permitting repeated normal invocations.
  Its init must explicitly set $01=$35 for REU I/O: loadutil enters it with
  $01=$34, which hides I/O. Omitting this caused signature rejection, fixed.
- install_native_utility.py uses native OPEN/CHROUT/CLOSE in the owned HDD,
  and verifies the utility with native LOAD via SYS (BASIC LOAD itself relinks
  arbitrary PRG bytes, so it cannot be used to relocate a comparison image).
  Utility filename PETSCII = D2 45 41 44 59 CF D3, displayed ReadyOS.
- Installer initially added a menu child without updating the encoded count
  H->I in '=;H'. This malformed utilities.m caused menu parsing to free zero
  pages, wrapping through the whole allocation table and then allocating page
  zero. Proven trace: C112 called X=0,Y=0 via 0FF8 reading $0383/$0384. Fixed
  header count, rebuilt/reinstalled and cold boot now reaches File Manager.
  Do not attribute that stall to the full-machine transfer.
- Latest build at this checkpoint: v0.5i, through normal wrapper --build-only.
  Native installed utility SHA 10e876d9b9e661ed1245d50513a17a61f0527a3c6e8ab2addb986327710ea538.
- Positive tests: one cold ReadyOS -> C64 OS -> ReadyOS return, then THREE
  consecutive warm round trips. Entire shim compared by app each return.
  Utility state $03FE is 0 after warm resume, proving normal close/reopen.
  Evidence: cold-fixed-menu.png, four-round-trips.png, native-menu-down.png.
- Native ReadyOS editor: typed unsaved 'BRIDGE STATE SEPTEMBER 25', returned to
  launcher, resumed C64 OS, selected ReadyOS from visible hamburger menu, then
  reopened editor: exact unsaved text remained. Saved as BRG0925A to runtime
  D81 through editor F5 dialog; modified flag cleared. This is a test-only file.
- Resumed C64 OS again and double-clicked device 8: directory of ReadyOS D81
  loaded through File Manager after warm resume. Evidence:
  c64os-d81-doubleclick.png. Current C64 OS view is that D81 directory.
- Bridge C app restarts main/BSS after returning through launcher, so cycle
  display resets; do not expect a count to continue across launcher visits.
- VICE wrapper now supports READYOS_BRIDGE_CONSOLE=1 for automated debugging.
  Current owned instance uses it, ports6511/6611; inspect PID before stopping.
  Desktop CUA was unavailable because Mac locked; native emulator API/UI event
  queue automation works without desktop access. It is not physical key input.
- C64 OS UI event queue: command PETSCII $07FA='R', modifier $07FD=7, count
  $0311=1 invokes ReadyOS normally. Mouse queue X=$07E8,Y=$07EE,kind=$07F4,
  count=$0310. Hamburger opens with X=1,Y=4,kind=1; track/up into first item
  X=10,Y=12,kinds2 then3 invokes ReadyOS. Device8 opens with X=24,Y=28,kind5
  (double-click). Mouse queue can hold six events, key command three.
- For monitor watchpoints keep the text connection alive; console mode avoids
  native monitor dialog capture after a disconnected breakpoint. To catch bad
  zero page counts, use `condition 1 if X == $00` (literal 0 alone was rejected).
  Native traces /tmp/trace-*.py and diagnostic images/VSFs in private environment.

Still required: formal repeatable two-OS regression (current experiments were
scripted ad hoc), disk-file readback and C64 OS state persistence checks, REU
partition comparison across real workloads, failure/invalid-state tests,
installer menu validation/readback, final docs and clean source commit. Do not
claim arbitrary SID/audio or active I/O suspension, and do not mark goal done yet.


## Completed native switching checkpoint (2026-09-25)

This checkpoint supersedes the outstanding-work lists above. Final normal
wrapper build: v0.5l; shared bridge record ABI 3 (`RBG3`). The eighth control
byte now carries the cold-load error. The X probe deliberately loads missing
`!OOTER`, reports KERNAL error 4, and returns to the complete ReadyOS context.

A false shim mismatch during that probe was traced to the single-byte REU
helper changing shim scratch byte $C83D. Bulk metadata reads/writes avoid that
scratch mutation. The comparison covers the complete $C600-$C9FF region.

Verified results:

- `python3 experiments/c64os/verify_roundtrip_ui.py`: PASS, three destructive
  ZP/stack/shim proofs, one cold and six warm round trips, failed cold recovery
  both before and after an existing C64 OS context, and native utility
  close/reopen. Unsaved Editor text survives. Native D81 save/reopen of
  BRG25TEST and C64 OS File Manager D81 reading/view preservation pass.
- Complete partition comparisons: 2,097,152 C64 OS bytes unchanged across
  ReadyOS work; 14,221,312 ReadyOS bytes unchanged across C64 OS work.
- `python3 experiments/c64os/verify_utility_guard.py`: PASS, missing snapshot
  rejected through the native utility lifecycle; all 14,680,064 bytes outside
  the C64 OS partition unchanged.
- `python3 experiments/c64os/verify_rejection_ui.py` with an actual 4 MB REU:
  PASS, unsupported configuration rejected and normal launcher restored.
- After exiting VICE and waiting for its PID to disappear, host c1541 readback
  of `brg25test,s` from roundtrip-final.d81 exactly equals
  `BRIDGE STATE SEPTEMBER 25` followed by CR. Live image copies can contain
  unflushed true-drive tracks and are not used for this assertion.
- Final SHA-256/size audit: every one of 7,230 files in both original and
  untouched baseline matches backup-manifest.json; no changed files.
- Full build and release directory ordering check pass (PREBOOT remains first).
  Python syntax and source diff whitespace checks pass.

The wrapper now refuses concurrent use of the private HDD and backs up its
runtime D81 before refreshing a changed release. The normal 16 MB visible
instance is restored after the 4 MB rejection test.

Portable results and exact native binary hashes are in verification.json;
private screenshots, diagnostic VSFs and reports remain in the sibling
experiment environment. No licensed C64 OS media or RAM snapshots enter Git.

Scope remains quiet text-mode Editor/launcher and C64 OS File Manager in VICE,
IDE64 v4.1, 16 MB REU. SID is silenced rather than restored; arbitrary custom
IRQ/CIA/SID drivers and active transfers are not certified. Device-8 mouse
activation uses the complete normal select/double-click sequence [1,3,4,1,3,5],
superseding the earlier incomplete kind-5-only note.


## TUI and expanded demo (2026-09-25)

The bridge now links the focused TUI core/window/menu/navigation/hotkey modules.
It has the normal framed header, arrow selector, Return activation, (c)/(s)/(x)
labels, and F2/F4/Ctrl+B footer. The existing F1 launcher alias remains. The
legacy status-bar declaration has no active micromodule implementation, so the
footer status line uses tui_puts_n, like the other current apps. No bridge RAM,
IDE64 transport or native C64 OS utility code changed; the native utility hash
remains 749358d4cd2679be07185f345b49f2d38b2de1c0f5cba4ad2f27b4aca2aa7cc7.

Normal build v0.5o and release directory ordering pass. verify_tui_ui.py passes
arrow/Return RAM proof, arrow/Return missing-boot recovery, C cold round trip,
F2/F4 cycles over Editor/REU Viewer/bridge (tokens 1,2,3), and Ctrl+B. The updated
verify_bridge_ui.py passes three destructive proofs and full 1KB shim equality.
The banner detection now checks the action text, so the launcher's app name is
not mistaken for the running bridge UI.

Editor and REU Viewer are preloaded before capture. The REU tour identifies
0-38 as skipped, 39 as the control bank, and physical bank42 / logical token3
as C64OS app state in this run. The demo uses normal native input events only.

The completed 107.2-second demo shows both round trips, task completion and
note persistence, plus the REU tour. Twenty-two screenshots accompany it.
Block By Block is added with a short fade-in and four-second fade-out.
See tui-verification.json for the final app hash and checks.


## Centered bridge layout (2026-09-25)

Centered the subtitle, return instructions and both gray footer help lines
using compile-time text widths. Removed the initial Choose an action prompt;
real restore/error status messages still use the bottom row. Normal wrapper
build v0.5p and release directory ordering pass. Native screen assertions
verify centered rows 1,10,11,22,23 and an initially blank row24. A cold round
trip passed while preparing the fresh demo; Editor and REU Viewer were
preloaded before recording.
