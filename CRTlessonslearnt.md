# CRT Lessons Learnt

Working notes for the ReadyOS EasyFlash + REU investigation.

## Proven So Far

- The EasyFlash shim must not be a fork.
  - The earlier `src/boot/easyflash_shim.s` was a reduced shim, not the full ReadyOS shim contract.
  - The canonical shim source is now shared in:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/readyos_shim.inc](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/readyos_shim.inc)
  - Both paths now consume that same shim source:
    - disk boot via [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/boot_asm.s](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/boot_asm.s)
    - EasyFlash shim binary via [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/easyflash_shim.s](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/easyflash_shim.s)
  - This is an architectural correction, not just cleanup.
- The current shared-shim EasyFlash launcher build definitely reaches launcher code.
  - Direct `-initbreak` checks in VICE now prove the current default CRT reaches:
    - `_main` at `$5253`
    - `_tui_getkey` at `$55BF`
  - That means the current CRT launcher is alive in the menu loop even when the user-visible screen output is still wrong.
- VICE headless screenshots are still not reliable as the sole ReadyOS CRT truth source.
  - Current `-exitscreenshot` captures can still show a flat pink screen even when breakpoint-based checks prove the launcher reached `_main` and `_tui_getkey`.
  - Live interactive VICE output remains the source of truth for this CRT path.
- The “wrong glyphs may just be PETSCII frame characters” hypothesis is still plausible, but not sufficient by itself.
  - I forced the CRT launcher toward:
    - explicit upper/graphics charset selection in `launcher_force_text_mode()`
    - plain-text EasyFlash header/status rows instead of PETSCII window boxes
  - Headless screenshot output still remained blank/pink after those changes, so the problem is not explained by frame glyph selection alone.

- The ReadyOS EasyFlash boot path improved materially after the copy/quiesce/stash rewrite.
  - The loader now:
    - copies payload bytes from cart to RAM
    - disables/quiesces the cart before REU commands
    - stashes launcher/apps/overlay metadata after the cart is out of the way
  - This moved the ReadyOS CRT path past the earlier “never reaches launcher” state.
- A CRT-only visual-probe launcher now renders successfully on screen under VICE.
  - Source toggle:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/launcher/launcher_easyflash.c](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/launcher/launcher_easyflash.c)
  - Screenshot proof:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/easyflash_visual_probe2.png](/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/easyflash_visual_probe2.png)
  - This bypasses catalog parsing, app-list setup, and REU launch logic, and it proves:
    - the CRT boot path can reach launcher code
    - visible text UI can render from the CRT launcher path
    - the remaining failure is in normal launcher initialization / app-state setup, not in the basic screen path anymore
- The embedded EasyFlash catalog load itself is good.
  - With the CRT probe limited to a single embedded app entry, the launcher can load the generated catalog data and render it on screen.
  - Screenshot proof:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/easyflash_visual_probe5.png](/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/easyflash_visual_probe5.png)
- The CRT runtime slot validator is not safe in the current launcher path.
  - Re-enabling `validate_slot_contract()` in the CRT probe caused immediate execution corruption again, even with only one embedded app entry.
  - Working interpretation:
    - disk-build runtime validation remains useful
    - CRT launcher should prefer build-time validation of generated metadata rather than rerunning the full runtime slot-contract validator in this path
- The CRT launcher’s custom row renderer is the unstable piece, not the shared TUI menu implementation.
  - The custom `draw_menu_item()` / `draw_menu()` path reintroduced blank screens or crashes.
  - Switching the CRT probe to the shared `tui_menu_draw()` path brought the launcher list back reliably.
- Full embedded catalog rendering now works in the simplified CRT launcher path.
  - Current proof screenshot:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/easyflash_visual_probe17.png](/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/easyflash_visual_probe17.png)
  - Proven combination:
    - cart copy/quiesce/stash boot path
    - embedded EasyFlash catalog
    - no CRT runtime slot validation
    - shared `tui_menu_draw()` instead of the custom launcher row renderer
    - normal description block rendering
- `set_shim_drive(8)` is safe in the CRT startup slice that renders the real launcher frame.
  - Enabling only `set_shim_drive(8)` on top of the stable visual-probe path still produced the real launcher header + app list in the remote monitor screen dump.
- The EasyFlash launcher debug bytes must not overlap the TUI hotkey table.
  - `TUI_HOTKEY_BINDINGS` occupies `$C7E0-$C7E8`.
  - Putting CRT debug stage bytes at `$C7E8` corrupts the last hotkey slot.
  - The CRT debug stage/mark bytes were moved out to `$C7EC/$C7ED`.
- The current CRT launcher payload spans two EasyFlash banks.
  - Current `bin/launcher_easyflash.prg` payload is about 30 KB, so it is no longer a one-bank toy payload.
  - The current cart copy routine already supports multi-bank payload copying, so launcher size alone is not the direct blocker.
- The startup regression after the stable visual probe came back specifically when extra CRT startup helpers were layered back in.
  - Safe, proven slice:
    - embedded catalog
    - shared `tui_menu_draw()`
    - `set_shim_drive(8)`
    - no CRT runtime slot validation
    - no startup REU bitmap sync
    - no startup hotkey seeding
  - Re-enabling `sync_from_reu_bitmap()` in the CRT startup path still destabilizes the launcher flow.
  - Re-enabling startup hotkey seeding on top of the stable visual-probe slice also regressed startup again in current experiments.
- Current working interpretation for CRT startup:
  - boot should remain the authoritative preload phase
  - launcher startup should prefer generated embedded metadata plus build-flavor assumptions over trying to reconstruct preload state through the old disk-launcher startup helpers
  - the next safe path is to restore actual launch behavior from the stable rendered launcher slice, rather than forcing all legacy startup helpers back in first
- The CRT launcher-to-shim REU handoff is now proven.
  - With a CRT-only autolaunch of the embedded `readme` entry, the runtime reaches `SHIM_LOAD_REU_RUN` at `$C803`.
- A fetched CRT app entry is also proven.
  - The same CRT autolaunch run reaches `readme` app `_main` at `$15E0`.
  - That means the EasyFlash launcher can hand off to the shim, the shim can fetch from preloaded REU, and the fetched app begins executing.
- The CRT launcher loop itself can run in the safe startup slice.
  - With:
    - embedded catalog
    - shared `tui_menu_draw()`
    - `set_shim_drive(8)`
    - no startup bitmap sync
    - no startup hotkey seeding
  - the launcher reaches `_tui_getkey`, so the non-frozen menu loop is viable in this slice.
- CRT launch behavior should not re-run the old runtime bitmap sync helper before each app launch.
  - In the EasyFlash flavor, boot preload is authoritative.
  - The CRT launch path now trusts the embedded/preloaded `apps_loaded[]` state instead of rebuilding it through `sync_from_reu_bitmap()` right before launching.
- Direct jump handoff from boot to `$1000` is safer than leaving an `$0800` handoff stub resident.
  - The old helper stub path produced a later `JAM` at `$080D`.
  - Replacing it with “restore launcher, disable cart, jump directly to `$1000`” removed that stub-specific crash mode and better matches the desired “copy in and get out of the way” cartridge model.

- Plain no-cartridge REU works in this VICE environment.
  - `bin/test_reu.prg` under VICE with REU enabled reaches its passing screen.
  - Screenshot proof:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/test_reu_baseline.png](/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/test_reu_baseline.png)
- A standalone no-cartridge host→REU→payload harness now works end to end.
  - host:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_standalone_host_asm.s](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_standalone_host_asm.s)
  - payload:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_payload_asm.s](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_payload_asm.s)
  - linker config:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/cfg/xefprobe_standalone.cfg](/Users/karlprosserpp/dev/c64projects/readyosprecog/cfg/xefprobe_standalone.cfg)
  - final screenshot proof:
    - [/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/xefprobe_standalone_fullpass.png](/Users/karlprosserpp/dev/c64projects/readyosprecog/obj/xefprobe_standalone_fullpass.png)
- The standalone harness now proves all of these, without cartridge involvement:
  - host PRG autostarts
  - host does REU stash/fetch round-trip for generated data
  - host stashes embedded payload bytes into REU
  - host fetches payload bytes back to `$1000`
  - host jumps into fetched payload successfully
  - payload fetches host-preloaded data from REU successfully
  - payload performs its own REU stash/fetch round-trip successfully

- The probe harness is the cleanest debugging surface right now:
  - boot: [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/boot_easyflash_probe_asm.s](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/boot/boot_easyflash_probe_asm.s)
  - host app: [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_host_asm.s](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_host_asm.s)
  - payload app: [/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_payload_asm.s](/Users/karlprosserpp/dev/c64projects/readyosprecog/src/apps/xefprobe/xefprobe_payload_asm.s)
- Cartridge boot and handoff are real and working:
  - the probe reaches `$1000`
  - the host app executes and draws its own screen
  - the old blank-blue failure was earlier startup/handoff trouble, not “never reached app code”
- The host app currently reaches its screen clear/title path and then fails at the first REU-touch path.
- The attached-cart debug ring currently ends as:
  - `SH012TJKGHIJLHC`
- The attached-cart host result bytes currently show:
  - `RESULT0 = 0x48`
  - `RESULT1 = 0x00`
  - `RESULT2 = 0xFA`
  - `RESULT3 = 0x50`
- That means the host entered its setup path and sampled `$DFxx`, but did not complete its local REU round-trip.
- Boot-side REU verification now exists in the probe boot.
- The attached-cart boot debug ring now shows:
  - `SH012BXTGHIJLHCQqW`
- The important new part is `BX`:
  - `B` = boot REU verification started
  - `X` = boot REU verification failed
- This means REU round-trip is already failing during boot preload, before the host app begins.
- Current boot debug bytes at `$C7E0-$C7E7` are:
  - `FA 50 FC 50 FC 50 FF 33`
- Interpretation:
  - `BOOTDBG0 = FA` after writing `$5A` to `$DF06`
  - `BOOTDBG1 = 50` from `$DF00` status sample
  - `BOOTDBG2/3 = FC 50` after stash command
  - `BOOTDBG4/5 = FC 50` after fetch command
  - `BOOTDBG6/7 = FF 33` meaning the first fetched byte was `$FF`, expected `$33`

## Important Artifact Handling Lesson

- Direct VICE runs that attach the real probe CRT path can mutate the CRT file on disk.
- This can silently flip the probe CRT header back from subtype byte `1` to `0`.
- The safe runner for repeatable experiments is:
  - [/Users/karlprosserpp/dev/c64projects/readyosprecog/build_support/temp_easyflash_probe_matrix.py](/Users/karlprosserpp/dev/c64projects/readyosprecog/build_support/temp_easyflash_probe_matrix.py)
- That runner now copies the CRT to a scratch runtime file before launching VICE.
- For header inspection, use:
  - [/Users/karlprosserpp/dev/c64projects/readyosprecog/build_support/analyze_crt.py](/Users/karlprosserpp/dev/c64projects/readyosprecog/build_support/analyze_crt.py)

## CRT Header Facts

- Freshly rebuilt probe CRT can be verified as:
  - hardware type `32`
  - subtype byte at `0x1A` = `1`
- But after unsafe direct runtime attachment, the source file may no longer remain in that state.
- The latest Ultimate docs explicitly define:
  - `C64:32 subtype 0 = Standard EasyFlash Cart ROM (uses ROM at $DF00-$DF1F)`
  - `C64:32 subtype 1 = REU-aware EasyFlash Cart ROM ($DF00-$DF1F not used)`

## Target Distinction

- The current evidence now points to an important split:
  - the chosen target format makes sense for Ultimate / Ultimate 64 according to the latest documentation
  - the current VICE 3.10 EasyFlash behavior still acts like standard type-32 I/O-2 ownership during runtime
- Working interpretation:
  - subtype `1` is a real target-level concept for Ultimate hardware
  - current VICE behavior does not appear to emulate the subtype-1 EasyFlash/REU coexistence model we need

## VICE-Specific Findings

- `-iocollision 2` did not change the attached-cart failure behavior.
- `detach 20` from the monitor is the correct cartridge detach command in monitor hex radix.
- `+cartreset` disables the automatic reset on cartridge attach/detach, but detaching still did not yield a clean “REU now works” continuation in the simple playback runs tried so far.
- Monitor resource syntax is:
  - `resourceget "NAME"`
  - `resourceset "NAME" "VALUE"`
- Direct manual proof from the current VICE manual text:
  - type `32` is `EasyFlash`
  - type `33` is `EasyFlash Xbank`
  - type `36` is `Retro Replay`
- In the VICE manual, the `REU compatibility bit` / `REU compatible memory map active` language belongs to the `Retro Replay` section, not the `EasyFlash` section.
- In the VICE manual, the `EasyFlash` section states that EasyFlash has 256 bytes of RAM mapped into the I/O-2 range.
- So within VICE’s documented model, attached EasyFlash type `32` and live REU access are in direct conflict at `$DFxx`.
- Emulator-assisted detach experiment results:
  - after `detach 20` and `resourceset "REU" "1"`, direct register reads become sane enough to read back `$DF06 = $5A`
  - but this does **not** restore working DMA in-session
- Manual monitor DMA check after detach + resourceset:
  - `m df00 df08` showed a sane register window
  - but a one-byte stash/fetch still failed to move data
  - so “REU register visibility” and “REU DMA actually functioning mid-run” are separate questions here
- Working interpretation:
  - VICE can expose the register block again after detach
  - but that still does not mean REU DMA becomes operational without a fresh machine start in this experiment path

## Updated Working Conclusion

- The conflict is not launcher-specific and not host-app-specific.
- Boot-time REU round-trip already fails while the EasyFlash cartridge is attached.
- Therefore the current blocker is the attached-cart hardware mapping model in VICE, not the later ReadyOS app logic.
- The no-cartridge technique itself is now proven.
- That means the remaining EasyFlash work is specifically about the cartridge transport / boot / coexistence path, not about whether REU preload + fetched app launch is conceptually viable.
- Update after the ReadyOS copy/quiesce rewrite:
  - the earlier probe conclusion is no longer the whole story for the actual ReadyOS CRT path
  - ReadyOS boot now reaches a visible CRT launcher visual probe under attached EasyFlash in VICE
  - so the current ReadyOS blocker has moved upward from “boot never reaches launcher” to “normal launcher init / app-state setup still breaks when re-enabled”

## Standalone Harness Lessons

- For assembly PRGs that should autostart like the existing boot/test programs:
  - keep `LOADADDR` in its own file-only memory area
  - do **not** map `LOADADDR` into the main RAM memory area
- When `LOADADDR` was incorrectly mapped into `RAM` for the standalone harness, the PRG layout shifted by 2 bytes and the startup stub behaved incorrectly.
- `cfg/xefprobe_standalone.cfg` should be treated as the working reference for this standalone harness shape.
- The first payload-side REU failure in the standalone harness was not a generic REU problem.
  - it was a memory-layout problem
- Two concrete payload mistakes mattered:
  - `FETCH_BUF` / `VERIFY_BUF` were originally under the BASIC ROM window at `$B000/$B100`
  - the payload scratch byte `expected` originally lived in linker `BSS`, and this payload config places `BSS` at `$B400`, also under BASIC ROM
- Moving payload working RAM out from under BASIC ROM fixed the payload-side verification path:
  - `FETCH_BUF = $C000`
  - `VERIFY_BUF = $C100`
  - `EXPECTED = $C200`
- This is highly relevant to ReadyOS CRT work:
  - a fetched app can be running correctly while still failing later because its scratch/data/buffer placement lives under ROM windows
  - “payload launched” and “payload can safely use its working buffers” are separate proof stages

## Immediate Next Steps

- Keep using the probe harness as the source of truth.
- Keep the standalone harness around as the known-good non-cartridge reference path.
- If continuing in VICE, focus on emulator-assisted or detach-style experiments only.
- Do not spend more time pretending the current attached type-32 EasyFlash run is “almost working” at the launcher level; the lower-level REU path is already proven broken first.
- If a “working thing” is required inside VICE, it likely needs a deliberately VICE-assisted flow rather than a pure attached-EasyFlash type-32 flow.
- If a “working thing” is required for the real target, subtype `1` should still remain the intended cartridge format unless stronger contradictory hardware evidence appears.

## Newer ReadyOS CRT Findings

- The shim contract is now source-identical between disk boot and EasyFlash boot:
  - `src/boot/boot_asm.s` includes `readyos_shim.inc` for the embedded disk-boot shim bytes.
  - `src/boot/easyflash_shim.s` also includes the same `readyos_shim.inc`.
  - So the current EasyFlash shim is no longer a separate fork.
- The current EasyFlash boot loader explicitly uses:
  - `cart_enable_for_copy()` before CRT ROM reads
  - `cart_disable_for_reu()` before every REU stash
  - a final `CART_CONTROL=$04` before launcher handoff
- The current EasyFlash loader no longer uses cc65-reserved zero page scratch:
  - loader scratch is in `$F0-$FF`, not `$02-$1B`
- The current EasyFlash launcher and the normal launcher still share the same cc65 app linker contract:
  - both use `cfg/ready_app.cfg`
  - `MAIN = $1000-$C5FF`
  - `__HIMEM__ = $C600`
  - `BSS` starts at `__ONCE_RUN__ + __ONCE_SIZE__`
- The current EasyFlash ReadyShell resident and the normal ReadyShell resident still share the same overlay-capable linker contract:
  - both use `cfg/ready_app_overlay.cfg`
  - resident image starts at `$1000`
  - overlay load window starts at `$8E00`
  - CRT behavior is isolated to the EasyFlash-only overlay transport micromodule
- The generated EasyFlash launcher catalog is now compile-time embedded and does not carry launch-drive metadata:
  - `readyos_easyflash_catalog.h` contains app banks, default slots, names, labels, and descriptions
  - the CRT launcher no longer needs `apps.cfg` for startup metadata
- Current launcher proof stage:
  - the EasyFlash launcher reaches `_main`
  - it also reaches `_tui_getkey`
  - so the current failure is after boot handoff and inside visible launcher runtime behavior
- Current visual truth:
  - live VICE output is still wrong
  - recent user-observed states include a garbage-text green screen and a plain green screen
  - headless screenshot capture remains unreliable for this path
- Working interpretation now:
  - the basic ReadyOS app memory contract is shared correctly
  - the shared shim requirement is now honored
  - the remaining issue is most likely in CRT-era runtime banking / launcher display-state interaction, not in the disk-vs-CRT metadata source choice alone

## Latest Verified ReadyOS CRT State

- Date: 2026-04-27.
- The older "VICE attached EasyFlash cannot do REU DMA at all" conclusion is superseded for the current ReadyOS CRT implementation.
  - It was true for the earlier minimal probe path.
  - It is not true for the current ReadyOS boot choreography after the copy/quiesce/stash rewrite and ROM-off handoff.
- `make xefprobe-standalone-verify` still passes.
  - The standalone no-cartridge REU harness remains the baseline proof that host-to-REU-to-payload execution works on this machine.
- `make easyflash-verify` now passes.
  - The target rebuilds `readyos_easyflash.crt` and `readyos_data.d64`.
  - It runs the memory-map verifier.
  - It verifies the EasyFlash release metadata and runtime-data-only data disk.
  - It runs the long warp VICE monitor path with preload verification enabled.
- The CRT is still emitted as the intended cartridge type.
  - `cartconv` verifies hardware type `32` (`EasyFlash`).
  - `cartconv` verifies subtype / hardware revision `1`.
- The current EasyFlash smoke verifier proves all preload records are byte-correct from REU.
  - Launcher bank 0 verifies.
  - All 16 app payload slots verify.
  - All 8 ReadyShell overlay slots verify.
  - The boot diagnostic block starts with `EFV\x01`, and failed records would now fail the smoke parser.
- The current handoff path is proven to be cart-free at launcher entry.
  - Boot restores the launcher from REU bank 0 into `$1000`.
  - EasyFlash is disabled before launcher jump with `$DE02=$04`.
  - The monitor dump sees the launcher reach `_main` and then the key loop.
- The current launcher startup is proven to use embedded metadata rather than disk metadata.
  - The CRT launcher uses `readyos_easyflash_catalog.h`.
  - It does not read `apps.cfg`.
  - It does not need disk launch-drive metadata for executable payloads.
- The current smoke verifier accepts PETSCII/high-bit uppercase debug markers.
  - cc65 character constants for uppercase letters can appear as high-bit PETSCII bytes in C64 screen/debug memory.
  - Treating those as ASCII-only caused false diagnostic failures.
- The current stable launcher IRQ rule is:
  - keep IRQ disabled through boot, cc65 startup, and launcher draw
  - enable IRQ only around the blocking `tui_getkey()` wait
  - disable IRQ again immediately after key read
- The CRT-only launcher post-link patch is currently required.
  - It removes cc65's startup `BSOUT $0E` call from `launcher_easyflash.prg`.
  - That KERNAL output path could hang with the CRT boot's IRQ/banking state.
  - The patch applies only to `bin/launcher_easyflash.prg`; the normal disk launcher is untouched.
- Memory-contract verification now documents intentional CRT diagnostic fixed addresses.
  - `$C7DF`, `$C7EC`, `$C7ED` are CRT/probe debug bytes.
  - `$D011`, `$D016`, `$D018`, `$D020`, `$D021`, `$DD00`, `$DD02`, and `$DBC0` are intentional VIC/CIA/color-RAM diagnostic/control touches in the CRT launcher path.
- Current verified bytes from the long smoke path:
  - REU magic at `$C700` is present.
  - preload bitmap at `$C836-$C839` is `FE FF 01 08`.
  - overlay metadata at `$C7F0` matches the generated layout.
  - storage drive remains `8`.
- Remaining work should move from "can the launcher boot" to app-runtime matrix debugging.
  - Editor is the known-good baseline observed interactively.
  - Tasklist, Quicknotes, CAL26, REU Viewer, Simple Files, and ReadyShell overlay behavior still deserve app-specific monitor checkpoints.
  - ReadyShell overlays remain last because ordinary app snapshot launch and runtime REU behavior should be kept proven first.

## Key-Loop RAM/REU Dump Findings

- Date: 2026-04-27.
- A repeatable key-loop dump script now exists:
  - `build_support/temp_easyflash_keyloop_dump.py`
  - It boots the current EasyFlash CRT in VICE warp mode with the drive-8 data disk mounted.
  - It dumps C64 RAM, visible I/O, and a VICE snapshot containing the full 16MiB REU.
  - It verifies launcher bank 0, all 16 app snapshots, and all 8 ReadyShell overlay payloads byte-for-byte against the generated layout.
- The first dump explained the "launcher visible but keyboard dead" symptom.
  - REU contents were already broadly correct.
  - EasyFlash was already disabled at the launcher key loop (`$DE02=$04`).
  - The preload bitmap/storage bytes were already correct (`$C836-$C839 = FE FF 01 08`).
  - But KERNAL IRQ/BRK/NMI vectors at `$0314-$0319` had been overwritten with spaces (`20 20 20 20 20 20`), which makes keyboard input nonfunctional.
- A first boot-side KERNAL/VIC normalization patch uncovered a second boot-layout hazard.
  - Growing boot code moved generated EasyFlash RODATA farther across `$1000`.
  - `clear_app_window` correctly clears `$1000-$C5FF`, so generated tables or shim bytes that spill into that range cannot be used in-place after app-window clearing starts.
  - The symptom was preload verification failures for launcher / late overlays, not a bad REU or bad CRT subtype.
- The proven boot-table fix is to copy generated layout tables to boot-only RAM before the first app-window clear.
  - App table copy: `$CC00`.
  - Overlay table copy: `$CC90`.
  - Preload and preload-verify now read those stable copies instead of the original RODATA labels.
  - This preserves the normal app memory contract because apps still restore only `$1000-$C5FF`, the shim remains `$C800-$C9FF`, and these boot scratch copies are not part of app runtime state.
- Current key-loop dump is clean after the fix.
  - All 16 app snapshots verify from REU.
  - All 8 ReadyShell overlays verify from REU.
  - Launcher immutable RAM verifies through RODATA.
  - KERNAL IRQ/BRK/NMI vectors are restored: `31 EA 66 FE 47 FE`.
  - EasyFlash is ROM-off at key loop: `$DE02=$04`.
  - CPU banking at key loop is `$0001=$36`, matching the RAM-under-BASIC posture needed by ReadyOS snapshots while I/O remains visible.
  - CIA1/CIA2 and VIC-visible dumps are captured in `obj/easyflash_keyloop_dump/report.json`.
- Follow-up keyboard finding:
  - Restoring only vectors was not enough; CINT/editor variables in page 2 can also be screen-cleared if KERNAL screen-base state is wrong.
  - The failing key-loop state had `$0289-$028F` as spaces (`20`), including `KEYLOG`, so cc65 `cgetc()` entered KERNAL keyboard code with a corrupt editor/keyboard block.
  - Boot now sets `$0288=$04` before `CINT` and explicitly reasserts the small keyboard/editor state block afterward.
  - The current verified key-loop state has `$0286-$028F = 0E 0E 04 0A FF 04 0A 00 00 48`.
  - `build_support/temp_easyflash_keyloop_dump.py` now fails if that block regresses.
