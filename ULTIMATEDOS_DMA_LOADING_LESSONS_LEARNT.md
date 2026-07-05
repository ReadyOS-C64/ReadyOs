# Ultimate DOS DMA Loading Lessons Learnt

This note records what was learned from the standalone C64 Ultimate DMA probe
and the launcher-local DMA experiments. It is specifically about Ultimate DOS
UCI technique; the launcher integration remains gated behind
`LAUNCHER_DMA_LOAD=1`.

## Current Proven Shape

- Use Ultimate DOS UCI calls for the actual DMA load path.
- Treat SoftIEC as a capability to detect/report, not as the loading transport.
- Put the probe program and payload files inside a mounted disk image.
- Mount the image as drive 8/A with the correct drive type (`d64` as 1541,
  `d81` as 1581).
- Load from the mounted image context, not from `/USB0/...` or `/USB1/...`
  paths in the final loading path.
- Use lowercase PETSCII directory names for probe payload files.
- Keep ReadyOS-side launcher filenames and C64U paths as lowercase PETSCII.
  BASIC/DOS may display those bytes as uppercase; do not normalize them to
  shifted-uppercase PETSCII.
- The DOS target expects ASCII file and directory bytes. For the lower-PETSCII
  ReadyOS names used here, preserving the raw bytes sends the equivalent
  uppercase ASCII spellings (`USB1`, `READYOS.D81`, `EDITOR`) that the C64U
  storage view and disk entries resolve. Do not force ASCII lowercase.
- Use fresh image names during automation to avoid stale mounted-image state.
- Prime the launcher DMA UI state from the configured image path at launcher
  startup. Do not show a lazy `DMA:??` state that depends on whichever action
  happens to ask about DMA first.
- The launcher startup check is intentionally path/config-only. A hardware run
  that probed the UCI command-interface ID during launcher init cleared the
  screen and never reached the first launcher draw. The actual load path still
  probes UCI before using Ultimate DOS; startup must not touch UCI.

The official Ultimate UCI documentation matters for the boundary here:

- Ultimate DOS target `$01/$02` has its own current-directory and open-file
  state. `DOS_CMD_LOAD_REU` (`$21`) reads from the currently opened Ultimate
  DOS file; it does not name an IEC drive.
- `DOS_CMD_COPY_UI_PATH` (`$15`) is documented as deprecated since firmware 3.0,
  so it cannot be treated as a production bridge from "whatever is mounted as
  drive 8" to the Ultimate DOS current directory.
- Control `CTRL_CMD_LOAD_REU` (`$04 $08`) loads an REU image file from storage
  into the Ultimate's REU memory. It is not an arbitrary per-file, per-offset
  app/resource DMA loader.
- Control `CTRL_CMD_GET_DRVINFO` (`$04 $29`) returns drive count plus
  type/bus/power triples. It does not expose the mounted image path that the
  launcher would need to synthesize an absolute Ultimate DOS path.
- The hyperspeed KERNAL reference implementation
  (`roms/c64rom/kernal/uci.s` in GideonZ/1541ultimate) is the protocol model:
  write the target/command bytes and payload bytes to `CMD_IF_COMMAND`, push
  the command through `CMD_IF_CONTROL`, wait for data/status state, then ack or
  abort explicitly. The launcher micromodule follows that raw command-interface
  pattern rather than using KERNAL or SoftIEC for the DMA transfer.

The fresh D81 artifact is:

```text
build/uci_dma_probe/uci_dma_probe.d81
sha256 f3f84122a7380635b9d30d6367893ddcab32d707b662a513b956e9fa3824be05
```

Its verified directory entries are:

```text
"probe"  prg
"udma1"  prg
"udma2"  prg
"udma3"  prg
```

## Working Probe Flow

The probe does this:

1. Detects UCI by probing the Ultimate command interface base addresses.
2. Reads and reports the SoftIEC bus byte.
3. Uses Ultimate DOS commands to access files from the mounted image.
4. Opens each payload file.
5. Reads/verifies the PRG load address header.
6. Seeks past the two-byte PRG header.
7. Uses Ultimate DOS `LOAD_REU` (`$21`) to DMA payload bytes into REU.
8. Fetches REU data back into RAM and verifies the expected byte patterns.
9. Writes compact result bytes and screen debug output for automation capture.

The current D81 C64U REST run reported:

```text
SUM USB:01 P:42 C:51 F1:55 F2:55 F3:55
PROBE DONE
```

`F1/F2/F3 = $55` proves that the batched Ultimate DOS `LOAD_REU` flow works
after Ultimate DOS has changed into the D81 filesystem. `P = $42` proves that a
plain filename stat before that directory change did not resolve as a valid
file stat, even though the same D81 was mounted as IEC drive 8/A. `C = $51`
proves that `COPY_UI_PATH` did not return a usable success status in this
REST-mounted D81 case, so it is not a proven pathless bridge.

The C64U REST automation that proved reliable uses this discipline:

- Upload the disk image by FTP.
- Reset to BASIC before mounting.
- Set Drive A type to match the image type.
- Mount the image explicitly with `type=d64` or `type=d81`.
- Reset to BASIC again after the mount.
- Type BASIC input one byte at a time.
- Wait for the keyboard buffer to drain between typed bytes.
- Wait for the probe screen to show `PROBE DONE`.
- Capture screen RAM and result buffers through REST.
- Analyze result bytes plus RAM/REU snapshots.

The VICE/dotnet sanity path is intentionally narrower:

- Build the D81 and mount it as drive 8 under VICE.
- Do not rely on `-autostart` deriving a useful filename from
  `uci_dma_probe.prg`; that tried `LOAD"UCI.DMA.PROBE",8,1`, while the disk
  file is named `probe`.
- Type `LOAD"PROBE",8,1` and `RUN` through the dotnet input sequence.
- Expect `UCI NOT FOUND` and validate the `$3000` result block as version
  `$41`, no-UCI mode.
- Use this only as a regression/smoke check. It cannot prove C64U Ultimate DOS
  DMA because stock VICE does not expose the C64U UCI device.

## Why Earlier Attempts Failed Or Misled Us

### SoftIEC Was The Wrong Loading Path

SoftIEC detection can be true on the C64U, but using SoftIEC as the load path
was not the thing we were trying to prove. The requested path is Ultimate DOS
DMA from files inside the mounted disk image.

SoftIEC is useful as a presence/capability indicator. It should not be the
launcher/app DMA transport unless a separate design explicitly chooses that.

### D81 Needed Correct Drive-Type Handling

The D81 case is more sensitive to the mounted drive type. The automation now
sets Drive A to 1581 for D81 and mounts with `type=d81`. Relying on a generic
mount state or a stale emulator/config assumption caused confusing failures.

### Reset Timing Mattered

The C64U state after mount was not always clean. The reliable automation resets
before the mount and again after the mount before typing `LOAD"PROBE",8`.

Without that, the machine could sit at BASIC, half-load, or show stale boot
screens that looked like code failure but were really automation/state failure.

### Keyboard Injection Was Too Coarse

Bulk-writing the C64 keyboard buffer was unreliable on the C64U. The probe
automation switched to one byte at a time and waits for `$C6` to drain after
each byte.

This made BASIC commands reproducible.

### REST Memory Polling Can Disturb KERNAL Loads

The ReadyOS C64U automation originally polled screen/RAM every second while the
booter was still inside KERNAL `LOAD "launcher",8`. That reproduced a stable
stuck `LOADING LAUNCHER...` screen. RAM captured during the stuck state matched
`launcher.prg` only through C64 address `$3C08`, and a second sample did not
advance.

After adding a quiet wait before the first REST memory capture, the same normal
`LAUNCHER_DMA_LOAD=0` D81 booted to the launcher and launched Editor. So do not
classify a boot-screen hang from automation unless the automation avoided REST
memory reads during active disk I/O.

### Stale Image Names Hid The Real Result

Reusing the same remote filename and mount state made it too easy to test an
old D64/D81 image while thinking a new one was mounted. The probe automation
uses distinct image names for D64 and D81 (`UCI40.D64`, `UCI41.D81`) and
rebuilds/uploads before testing.

### Case Was Easy To Misread

C64 BASIC displays uppercase for text that may actually be lowercase PETSCII.
The disk builder verifies the raw directory bytes for lowercase PETSCII names,
not just what the screen or `c1541` display appears to say.

This also applies to the launcher-side C64U image path. The launcher must
preserve lowercase PETSCII path bytes such as `/usb1/readyos.d81` while parsing
ReadyOS config/catalog data and while emitting the current DOS-target command
bytes. Those byte values are valid ASCII uppercase spellings for the C64U path
and disk entries. Converting the ReadyOS strings to shifted-uppercase PETSCII or
ASCII lowercase is not equivalent.

### Launcher Experiments Drifted From The Probe

The standalone probe used the known-good Ultimate DOS sequence. A later launcher
experiment drifted from that sequence and also introduced startup/boot noise,
making the result hard to interpret. Launcher integration should stay directly
aligned with the proven probe sequence, with any deviation verified on hardware.

## Rules For Launcher Integration

- Start from the working D81 probe sequence, not from a new interpretation.
- Keep all launcher changes behind a small, obvious feature boundary until
  hardware proof exists.
- Treat the config path as the startup UI gate (`DMA:YES`/`DMA:NO`). The first
  actual app/resource load probes UCI, performs the Ultimate DOS transaction,
  and changes the status to `DMA:ON` only after a successful DMA load.
- Persist the `DMA:ON` display bit through the launcher resume payload. The
  mounted-image-ready bit stays process-local and is re-established by the next
  Ultimate DOS load, because apps may leave the DOS target in a different
  state.
- When DMA is positively available, do not silently fall back to KERNAL/chunked
  loading after an Ultimate DOS load failure. Falling back is still correct when
  UCI is unavailable, but on C64U hardware a DMA failure must stay visible so we
  do not confuse a slow disk preload with a working DMA preload.
- Do not insert helper routines into the middle of the proven UCI transaction
  code. One hardware build hung on the first cold DMA load at stage `250.3834`
  after a helper was inserted before the DOS identify path. Moving helper code
  to the end of the code segment restored the original transaction layout and
  keeps future diffs easier to audit.
- Any settle/quiesce helper should run only after a transaction path has
  returned. It should synchronize the command interface and wait for idle; it
  is not a startup probe and it should not touch the mount/path state.
- Do not mark an app as loaded unless the Ultimate DOS command status and REU
  verification path prove the data landed.
- Keep the shim, boot, and app binaries out of scope.
- First prove one standalone app payload, then ReadyShell overlay/resource
  payloads, then ReadyBASIC/ReadyShell launch behavior.

### Main App Snapshot Size Rule

The Ultimate DOS `LOAD_REU` byte count and the ReadyOS shim restore byte count
are different pieces of state:

- `LOAD_REU` should use the exact PRG payload length reported by Ultimate DOS
  `FILE_INFO` minus the two-byte PRG load header, after seeking to payload
  offset 2. Loading a fixed window length from a shorter file is not a safe
  EOF strategy.
- For normal DMA-loaded app PRGs, the launcher records the exact payload size
  for the shim REU launch, matching the pre-existing launcher/shim behavior.
- A tested ReadyShell hypothesis tried clearing the full target snapshot window
  and then asking the shim to restore the full `$B600` ReadyOS app window. On
  C64U hardware, that moved the failure earlier: the DMA preload still reported
  `DMA:ON`, but launching ReadyShell dropped to BASIC `READY.` instead of
  drawing the shell UI.

Current intentional guardrail:

- Ultimate DOS DMA remains enabled for normal main app PRGs.
- ReadyShell main PRG DMA is enabled when the experimental launcher is built
  with `LAUNCHER_DMA_LOAD=1`; the top-level build gate still defaults off for
  normal builds.
- ReadyShell overlay/resource PRG DMA is also enabled under
  `LAUNCHER_DMA_LOAD=1`, with KERNAL/chunked fallback if the Ultimate DOS call
  fails.
- This must stay covered by C64U hardware UI automation. VICE can prove normal
  launcher behavior, but not the Ultimate DOS timing and mounted-image path.

## Temporary Launcher Config Bridge

The experimental launcher now has a deliberately temporary config key:

```text
[launcher]
c64u_image_path=/usb1/readyos.d81
```

This is read from the normal `apps.cfg` path and cached inside the launcher.
When `LAUNCHER_DMA_LOAD=1`, the launcher passes that string to the
launcher-local Ultimate DOS micromodule before each DMA load. The current split
for `/usb1/readyos.d81` is:

1. `CHANGE_DIR /`
2. `CHANGE_DIR /USB1`
3. `MOUNT8 readyos.d81`
4. `CHANGE_DIR readyos.d81`
5. `OPEN` plain filenames such as `editor`, `readybasic`, `rsparser`, and
   `rbcore`
6. `READ` the two-byte PRG header and validate the expected load address
7. `SEEK` to payload offset 2
8. `LOAD_REU` the known destination window/slot length

All names sent to the Ultimate DOS target in those commands must be ASCII
bytes. The launcher currently preserves the ReadyOS lowercase PETSCII byte
values because, for these names, they are also the uppercase ASCII spellings the
C64U resolves.

For the current hardware experiment, the DMA-enabled launcher also seeds the
same path as its experimental default before reading `apps.cfg`. The config key
should still be the intended control point, but the default avoids blocking the
DMA transport test on a config-parse/cache issue.

This is not the final desired design. It exists to verify the rest of the
launcher DMA loader against real hardware while the pathless "currently mounted
drive 8/A image" bridge remains unproven. The test image uploaded by automation
must use the same C64U-side name as the config value, currently
`/usb1/readyos.d81`. The known-good probe used short image names and paths
without a trailing slash, so the launcher test should match that spelling.

One important mount nuance remains: `MOUNT8` may transport successfully while
returning no status bytes. The standalone probe did not require a status packet
immediately after mount; it treated the following `CHANGE_DIR` into the mounted
image as the validation step. The launcher micromodule follows that discipline
and does not fail the mount solely because the status buffer is empty.

## Launcher Size / Memory Impact

Measured on the experimental branch after rebuilding only `bin/launcher.prg`:

```text
default launcher:
  PRG bytes 38902
  CODE   $1033..$A1E7
  RODATA $A1E8..$A77F
  DATA   $A780..$A7B1
  ONCE   $A7CE..$A7F3
  BSS    $A7F4..$B185

LAUNCHER_DMA_LOAD=1 fixed-window build:
  PRG bytes 42171
  CODE   $1033..$AE78
  RODATA $AE79..$B444
  DATA   $B445..$B476
  ONCE   $B493..$B4B8
  BSS    $B4B9..$C023
```

The default build was rebuilt after this measurement and confirmed to have no
`launcher_uci_dma` symbols in `obj/launcher.map`.

## Launcher Integration Follow-Up

The launcher integration remains experimental and disabled by default.

What changed the interpretation:

- The working probe was not a proof that plain Ultimate DOS filenames resolve
  through IEC drive 8/A's mounted image. Ultimate DOS has its own current
  directory state.
- The revised probe made this explicit after forcing Ultimate DOS to `/` so
  sticky state from prior runs could not mask the result: pre-CD `STAT "udma1"`
  returned `P:42`, `COPY_UI_PATH` then returned `C:51`, while the later
  CD-into-image batch load still returned `F1:55 F2:55 F3:55`.
- REST mounting a D81 as drive A made IEC/KERNAL access see the image, but did
  not make launcher-side Ultimate DOS `OPEN`/`STAT` reliably resolve `editor`
  from that mounted IEC context.
- Adding Ultimate DOS `COPY_UI_PATH` before `STAT`/`OPEN` did not make the
  REST-mounted launcher case work. The probe now records this as `C:51`. It may
  still be useful when a human has navigated/mounted through the Ultimate UI,
  but that is not proven.
- The successful launcher shape uses the explicit config bridge:
  `CD /`, `CD usb1`, `MOUNT8 readyos.d81`, `CD readyos.d81`, then plain
  filenames.
- After those failures, normal builds were changed so `LAUNCHER_DMA_LOAD`
  defaults to `0`. The UCI assembly module links only when explicitly building
  with `LAUNCHER_DMA_LOAD=1`.

Current safe conclusion:

- Do not ship or publish the launcher UCI DMA path without hardware proof for
  the app/resource set being enabled.
- A pathless, production-safe Ultimate DOS load from "whatever disk image is
  mounted as drive 8" is not proven.
- The currently documented UCI commands do not expose a mounted-drive-to-DOS-path
  bridge. The only proven DMA path is after Ultimate DOS itself has changed into
  the disk-image filesystem.
- If the final design must avoid absolute `/USB0` or `/USB1` paths and must not
  use SoftIEC, the missing piece is a proven Ultimate DOS mechanism that binds
  its filesystem context to the currently mounted IEC image.
- Until that exists, the normal shim/KERNAL loader path is the safe launcher
  behavior.

## C64U Launcher Automation Notes

The C64U REST automation can prove useful states, but failures must be
classified from the captured screen:

- A long `LOADING LAUNCHER...` screen with changing animation is not
  necessarily a lockup. On a known-safe `LAUNCHER_DMA_LOAD=0` D81, the launcher
  appeared after a brief blank transition.
- A final BASIC `READY.` screen after `LOAD"BOOT",8` means the booter did not
  actually run in that attempt. Treat this as mount/input/run sequencing noise,
  not launcher init evidence.
- For app-launch checks, wait for app-specific text such as `EDITOR:`. Matching
  the launcher menu row `8 EDITOR` is a false positive.

## Launcher No-STAT / Fixed-Window Experiment

The launcher integration exposed a C64U-side hazard that the standalone probe
did not originally make obvious: after `CD`/`MOUNT`/`CD image`, issuing Ultimate
DOS `FILE_STAT` for `editor` consistently stopped at the launcher's debug marker
`55` and the C64U REST endpoint stopped responding.

The current experimental launcher path therefore avoids `FILE_STAT` entirely:

- open the file by Ultimate DOS name
- read and validate the two-byte PRG load address
- seek back to payload offset 2
- zero the destination REU window/slot first
- issue Ultimate DOS `LOAD_REU` for the known destination window/slot length

For main app snapshots, the destination window is `$1000-$C5FF`, length
`$B600`. Exact PRG EOF size is not required because the target REU window is
cleared first and the shim can restore the full app window. ReadyShell overlays
also have a fixed slot length (`$3800`), so the same strategy applies there.
Only dynamically packed resources without a known slot length would require a
reliable size query or EOF scan.

Current measured launcher map impact, forced rebuilds:

```text
default launcher:
  CODE   $1033..$A1E7 size $91B5
  RODATA $A1E8..$A77F size $0598
  DATA   $A780..$A7B1 size $0032
  ONCE   $A7CE..$A7F3 size $0026
  BSS    $A7F4..$B185 size $0992

LAUNCHER_DMA_LOAD=1 fixed-window build:
  PRG bytes 42171
  CODE   $1033..$AE78 size $9E46
  RODATA $AE79..$B444 size $05CC
  DATA   $B445..$B476 size $0032
  ONCE   $B493..$B4B8 size $0026
  BSS    $B4B9..$C023 size $0B6B
```

## Final Launcher Hardware Result

The launcher DMA path is now proven on a C64U for the regular D81 profile when
the D81 path is supplied in launcher config as `c64u_image_path=/usb1/readyos.d81`
and the launcher is built with `LAUNCHER_DMA_LOAD=1`.

The working Ultimate DOS sequence is:

```text
DOS target only, not SoftIEC
CD /
CD USB1
MOUNT8 READYOS.D81
CD READYOS.D81
OPEN plain file name
READ 2-byte PRG load address
SEEK payload offset 2
LOAD_REU fixed destination length
```

The important correction was to mirror the working probe: after `CD /`, change
Ultimate DOS into `USB1`, then mount and enter the image by its short image name.
Mounting with an absolute path and then trying `CD READYOS.D81` was not enough
on the tested C64U; it returned `21,UNKNOWN`.

The Launcher does not need exact app file sizes for main app snapshots. It
zeros the REU destination window first, validates the PRG load address, and
then asks Ultimate DOS to `LOAD_REU` the full fixed app window (`$B600`) into
the resolved physical REU bank. The shim app size is set to `$B600`. ReadyShell
overlays use the same strategy with their fixed `$3800` overlay slot.

Hardware evidence from C64U REST automation:

- Final rebuilt artifact `readyos-v0.2.5u-d81.d81`, Editor: load dialog showed
  `OK - 45 KB`, launcher showed `DMA:ON`, and Editor opened.
- `readyos-v0.2.5d-d81.d81`, Editor: load dialog showed `OK - 45 KB`, launcher
  showed `DMA:ON`, and Editor opened.
- `readyos-v0.2.5z-d81.d81`, ReadyShell: load dialog showed `OK - 45 KB`,
  launcher showed `DMA:ON`, and ReadyShell reached `READYOS READYSHELL` /
  `LOADING DONE`.
- `readyos-v0.2.5d-d81.d81`, ReadyBASIC: load dialog showed `OK - 45 KB`,
  launcher showed `DMA:ON`, and ReadyBASIC reached its `READY.` prompt.

One C64U quirk remains in the launcher implementation: the successful hardware
shape keeps volatile stage/status writes during the UCI transaction. They are
cleared from the normal success dialog before `OK - 45 KB` is drawn, but if DMA
fails they intentionally remain as short diagnostic breadcrumbs on the load
dialog. Removing those writes made the same logical path fall back to the
KERNAL loader on hardware.

### UCI Timing / Code-Shape Sensitivity

This is the most surprising lesson from the Launcher integration. The UCI
loader is not merely "logically correct"; on the tested C64U it is sensitive to
the exact pacing/code shape around the Ultimate DOS transaction.

External research context:

- The official UCI documentation describes UCI as a small state machine exposed
  through `$DF1C-$DF1F`. Commands may only be pushed while the interface is
  idle. After the Ultimate prepares a response, the C64-side code must read any
  data/status bytes it needs and then write `DATA_ACC` to release the response
  and return the state machine to idle or busy for the next block.
- The upstream `roms/c64rom/kernal/uci.s` code is conservative even though it
  targets the SoftIEC/KERNAL path rather than Ultimate DOS. Before each command
  setup it aborts any pending command, pushes the new command, waits while the
  interface is busy, acknowledges data with `DATA_ACC`, and waits for the
  accept bit to clear before continuing.
- The official Ultimate64 firmware notes for `3.14d` dated 2026-03-01 include
  "Fixes race condition in UCI" under general bugfixes. Treat plain `3.14.0`
  / Commodore `1.0.0` separately from `3.14d`; the Commodore `1.1.0` changelog
  says earlier `3.14.0` is the initial `1.0.0` firmware, but does not call out
  that UCI race fix by name.
- Public C64U/Ultimate documentation also distinguishes Software IEC from UCI.
  Software IEC is the serial-bus virtual drive path. The launcher DMA path must
  remain on the Ultimate DOS UCI target, where `LOAD_REU` reads from the
  currently opened Ultimate DOS file into REU.

Observed hardware evidence:

- With stage/status stores to screen RAM (`$052C-$0533`) in place, the launcher
  DMA path works on C64U.
- Removing those stores made the same logical path fall back to the normal
  KERNAL loader. The load dialog showed `OK - 24 KB`, the launcher showed
  `DMA:NO`, and no Ultimate DOS REU load was used.
- Replacing the stores with private BSS breadcrumbs did not restore the working
  behavior.
- Restoring the screen stores restored DMA. The success dialog is now cleared
  wide enough that the user sees clean `OK - 45 KB`; on failure the diagnostic
  bytes remain visible.

Interpretation:

- Treat this as **C64U UCI transaction pacing/code-shape sensitivity**, not as a
  fully understood protocol rule.
- The current best guess is that the extra absolute stores provide enough
  spacing and/or preserve enough code-generation shape between command setup,
  UCI status polling, response draining, path changes, open/read/seek, and
  `LOAD_REU` for the C64U firmware/FPGA state machine to remain happy.
- It may also relate to where the writes go. Private BSS writes were not an
  equivalent substitute, so this is not proven to be "any delay will do".
- It is also possible that the C compiler/assembler layout and register usage
  around the C/asm boundary is part of the sensitivity. The screen writes are
  volatile absolute stores, so they constrain generated code more strongly than
  ordinary local/state writes.
- The screen writes are not part of the payload path. The payload path is:
  Ultimate DOS file -> `LOAD_REU` -> REU. The screen writes are small volatile
  breadcrumbs around command setup/status capture and may be changing timing,
  bus visibility, code layout, or C/asm boundary behavior enough to avoid a UCI
  race on the tested firmware.

Rules for future changes:

- Do not remove or relocate the `$052C-$0533` stage/status writes unless the
  replacement is retested on real C64U hardware.
- If the visible diagnostics become undesirable, prefer clearing the affected
  row after success, as the current code does, rather than removing the stores.
- If we want a cleaner design, build a dedicated UCI pacing primitive and prove
  it on hardware with Editor, ReadyShell, and ReadyBASIC before replacing the
  screen breadcrumbs.
- A cleaner pacing primitive should copy the upstream discipline rather than
  merely burning cycles: on entry, clear errors/abort stale transactions; before
  each command, drain data/status queues and require stable idle; after each
  response, write `DATA_ACC`, wait for `DATA_ACC` to clear, then require stable
  idle before the next command.
- Do not use the destination slot size as the `LOAD_REU` length unless the file
  is known to be exactly that size. ReadyShell overlays are short PRGs packed
  into fixed `$3800` REU slots; the normal KERNAL/resource path reads until EOF
  and leaves the pre-zeroed tail alone. The DMA path must query Ultimate DOS
  `FILE_INFO`, subtract the two-byte PRG header, then `SEEK 2` and `LOAD_REU`
  exactly that payload length.
- A launcher experiment with named `FILE_STAT` failed on C64U firmware 3.14 in
  the mounted-D81 path with stage `6`, `ERR_STAT`, debug `84`, before the main
  ReadyShell PRG opened. Do not switch the launcher sizing path from
  open-handle `FILE_INFO` back to named `FILE_STAT` without reproving it on
  hardware.
- ReadyShell has edge-of-slot overlays such as `rsfops`, whose real PRG payload
  fits in `$3800` but is close enough that rounded/allocated-size answers can
  trip the size guard. Preserve reported size bytes in diagnostics before
  closing the Ultimate DOS handle; `CLOSE` can overwrite the status/debug bytes.
- On the C64U D81 run, `rsfops` failed at stage `7` with `ERR_SIZE` and
  reported size bytes `$8E,$38` (`$388E`). That matches 57 D81 blocks times
  254 bytes, not the actual host PRG size. The launcher therefore allows an
  overlay-only clipped transfer when the target is the ReadyShell overlay slot
  (`load address $8E00`, slot `$3800`) and the reported size is within the
  next 512-byte rounding window. Main app PRGs still require exact reported
  sizes.
- Do not trust restored `app_resource_loaded` state when a main app is freshly
  preloaded. A C64U reboot can preserve launcher bank-0 state while the REU
  cache banks contain stale contents from a previous experiment/session. The
  failure signature was ReadyShell drawing its header and `LOADING DONE`, then
  hanging before the `>` prompt because overlay 9 (`rsedit`) metadata pointed at
  a stale app snapshot bank. The launcher fix is to invalidate resource preload
  state on every fresh main-app load, reload the resources, and reassert all
  ReadyShell cache/state bank allocation types before restoring metadata.
- Do not blindly reuse restored `app_rs_bank*` values. On C64U cross-app runs,
  Editor and ReadyBASIC could load correctly, ReadyShell could reach its prompt,
  and then an overlay command such as `LST "RSHELP"` could drop to BASIC if the
  restored ReadyShell resource bank slots conflicted with banks now marked for
  another app/resource type. Before reusing restored resource-bank slots, compare
  them against the allocation table, discard conflicting or duplicate slots, and
  allocate fresh banks before reloading overlays/modules.
- ReadyShell currently clears its high-RAM runtime area at startup, which can
  clear the launcher's `$CFF2` cached state-bank pointer before ReadyShell asks
  for the REU state bank. In that case ReadyShell scans the allocation table for
  the first `REU_RS_SCRATCH` bank. The launcher therefore must prune stale
  ReadyShell cache/scratch allocation-table marks so only the current
  ReadyShell cache banks and state/scratch bank are advertised before launch.
- Keep the REU allocation table authoritative for loaded app snapshots as well
  as resource banks. The DMA preload path updates the shim bitmap directly; it
  must also mark the loaded app's physical REU bank as `REU_APP_STATE`, and the
  launcher should reassert all currently loaded snapshot banks before allocating
  resource banks. Otherwise ReadyShell overlay banks can be allocated over a
  loaded app snapshot; later returning from that app can overwrite an overlay
  bank and make commands such as `LST "RSHELP"` fail even though the ReadyShell
  prompt and lighter commands still work.
- On this experiment branch, Enter on an unloaded app should go through the
  launcher preload path first, then launch from REU. That keeps DMA-capable app
  loads, KERNAL fallback loads, and resource preparation on the same launcher
  code path as F3/load-all instead of using the older direct disk-launch path.
- Do not poll screen/RAM through the REST API while the C64 side is in the
  boot/UCI-critical section. Earlier automation did this and produced stuck
  boot screens that looked like launcher crashes but were really
  automation/state interference.
- Record the exact firmware version when testing. Behavior on `3.14.0`,
  `3.14d`, Commodore `1.0.0`, and Commodore `1.1.0` may differ in precisely the
  area this loader stresses.
- VICE cannot prove this aspect. The failure mode only showed up on hardware.

Final measured launcher map impact after the verified build:

```text
LAUNCHER_DMA_LOAD=1 verified build:
  PRG bytes 43521
  CODE   $1033..$B3B1
  RODATA $B3B2..$B98A
  DATA   $B98B..$B9BC
  INIT   $B9BD..$B9D8
  ONCE   $B9D9..$B9FE
  BSS    $B9FF..$C56E
```

Verification:

- `python3 build_support/verify_memory_map.py` passed.
- `python3 build_support/verify_launcher_dma_gate.py` passed.
- C64U D81 artifact `readyos-v0.2.5q-d81.d81` was deployed as
  `USB1/automation/vice_tasks_dotnet/readyos-readyshell-c64u-q.d81`; the FTP
  listing was checked so no older experiment D81 from this series remained.
- F3/load-selected ReadyShell path passed:
  `READYOS_READYSHELL_OVERLAY_SMOKE_PASS`. The captured loading screen showed
  `OK - 27 KB`, then ReadyShell passed `VER`, `LST "RSHELP"`,
  `CAT "RSHELP" ! TOP 1`, and another `VER`.
- Enter on unloaded ReadyShell path passed:
  `READYOS_READYSHELL_ENTER_SMOKE_PASS`. The five-second early capture showed
  `LOADING TO REU` with the ReadyShell selection and a UCI stage marker, then
  ReadyShell passed the same overlay command smoke.

## Launcher Startup/Resume Lessons

- The launcher may reuse its cached catalog/resume payload instead of rereading
  `apps.cfg` on every entry. Any config value that changes DMA behavior must be
  part of that resume payload, or the cached path can silently fall back to the
  compiled default. This specifically bit `c64u_image_path`: after a cached
  launcher resume, first Enter/F1 paths could try the wrong image path and then
  fall back to KERNAL disk I/O.
- Bump the launcher resume schema whenever the resume payload layout changes.
  That forces old hardware REU cache state to be discarded and makes the next
  launcher run reread `apps.cfg` before it saves a new payload.
- DMA status should be initialized during launcher startup without touching the
  UCI registers. A real startup UCI detect can run too early in the
  boot-to-launcher lifecycle and hang before the first menu draw. `DMA:YES`
  therefore means a C64U image path is configured and the launcher will attempt
  DMA; `DMA:ON` means at least one Ultimate DOS DMA transfer has succeeded in
  the current launcher session; `DMA:NO` means no path is configured or a later
  no-UCI/path/mount/image error disabled the DMA path.
- A failed per-file transfer should not automatically disable DMA globally.
  Only no-UCI/path/mount/image errors should flip global status to unavailable.
  Size/header/open/read errors should fall back for that file while allowing
  later files to attempt DMA.
- Repeatedly remounting the same configured D81 for every app and overlay is
  risky on the tested C64U. After one successful Ultimate DOS DMA load, the
  launcher keeps a volatile in-session "image is mounted" flag and subsequent
  DMA transfers open the next file from the current image directory instead of
  issuing another root/CD/mount/CD-image sequence. The flag is not saved in the
  resume payload and is cleared on startup and on no-UCI/path/mount/image
  failures, so a retry can fall back to the full mount path when the mounted
  image is genuinely suspect. File-level failures keep the mounted-image state:
  ReadyShell can legitimately fall back to KERNAL for an overlay/resource while
  Ultimate DOS is still positioned inside the D81, and forcing the next app back
  through the full mount path reproduced the `330.3834` C64U stall.

## 2026-06-27 Launcher DMA Verification

- Current tested branch build: `readyos-v0.2.5z-d81.d81`.
  Local and FTP-downloaded `USB1/readyos-v0.2.5z-d81.d81` both hashed to
  `42fd4f6e148aa5cab95972199324174acb807e219ff102c71b0eb444dab87c54`
  before the hardware editor-direct run and again after the dotnet C64U suite.
- `apps.cfg` for this run deliberately used
  `c64u_image_path=/usb1/readyos-v0.2.5z-d81.d81` and
  `load_all_to_reu=0`, so the booter did not auto-preload everything. The
  human/user-world rule remains: the D81 can be mounted normally by the user,
  while launcher DMA only needs a configured C64U storage path when it must
  remount/recover the image itself.
- The C64U path handling now sends Ultimate DOS storage path bytes in uppercase
  for `CD`/`MOUNT` only, and leaves D81 file names for `OPEN` untouched. That
  preserves the PETSCII/lowercase app/resource filename discipline inside the
  mounted image while satisfying C64U storage-path matching for `/USB1`.
- The working mount recovery shape is `CD /`, then try `CD <dir>` exactly as
  configured, and if that status fails and the configured directory starts with
  `/`, retry once without the leading slash. This fixed the previous
  `ERR_PATH`/status `84` failure for `/usb1/...`.
- Do not add temporary top-left screen markers to launcher startup. They helped
  diagnose draw progress, but they are not part of the working loader and can
  look like the classic C64 blinking-cursor failure symptom. The remaining
  `$052C-$0533` writes are the proven UCI pacing/status breadcrumbs inside the
  loading screen area and are cleared on successful DMA.

Measured launcher map for the verified `0.2.5Z` build:

```text
LAUNCHER_DMA_LOAD=1 verified build:
  SHA256 bin/launcher.prg 50e00ec57be0f2905ae25a003c2bcc58c539110fe22d630031a49ccb8ebd7ceb
  CODE   $1033..$B380
  RODATA $B381..$B957
  DATA   $B958..$B989
  INIT   $B98A..$B9A5
  ONCE   $B9A6..$B9CB
  BSS    $B9CC..$C3F6
```

Verification on C64U:

- Static gates passed: `python3 build_support/verify_memory_map.py` and
  `python3 build_support/verify_launcher_dma_gate.py`.
- Shell/REST editor-direct smoke passed with
  `READYOS_EDITOR_DIRECT_PASS`. It booted the fresh D81 from `USB1`, reached
  the launcher with `DMA:YES`, pressed Enter on Editor, showed the REU loading
  progress, and landed in Editor.
- Dotnet C64U cross-app suite passed with `"Status": "success"` using
  `/tmp/readyos_c64u_readyshell_dotnet_z3_20260627_131943` and trace
  `logs/ultimate_auto_20260627_131952/trace.md`. It verified launcher
  `DMA:YES`, direct Editor load returning to launcher as `DMA:ON`, ReadyBASIC
  load returning as `DMA:ON`, and ReadyShell reaching its prompt after
  ReadyBASIC.
- The same dotnet suite exercised ReadyShell overlays with `VER`,
  `LST "RSHELP"`, another `VER`, and `CAT "RSHELP" . TOP 1`; the decoded
  capture showed `NAME:RSHELP,BLOCKS:5,TYPE:SEQ` and `VERSION 0.2`, so the
  previous ReadyShell overlay preload hang was not reproduced in this build.

## 2026-06-29 Launcher-Shape UCI Timing Probe

- The timing probe must use the launcher loader shape, not the older standalone
  named `FILE_STAT` probe. The working shape is still: detect UCI, identify,
  drive-info, optional root/CD/mount/CD-image, open filename inside the mounted
  image, `FILE_INFO` on the open handle, read the two-byte PRG load header,
  seek to payload offset 2, `LOAD_REU`, close.
- For per-stage timing, the probe now generates an instrumented copy of
  `src/apps/launcher/launcher_uci_dma.s` at build time. The product launcher
  source is not modified. The instrumented wrappers preserve the underlying
  carry result with `php`/`plp`, so the launcher loader control flow remains
  equivalent while accumulating stage timings in the probe result block.
- Use the low/mid jiffy bytes (`$A2/$A1`) for timing. Reading `$A0/$A1`
  produced coarse `$0100`-tick steps and made stage timings look falsely
  quantized.
- C64U run `USB1/UTV201111.D81` completed 29/29 launcher-style DMA loads:
  `DONE:01`, `FAIL:00`, `SUCCESS:1D`, `loaded_kb=611`, `total_ms=83167`.
- Stage timing for the 29 loads, including the first-load mount path and
  subsequent mounted-image opens:

```text
load_reu       11.883s  14.3%
open           10.733s  12.9%
file_info      10.583s  12.7%
identify        9.717s  11.7%
read_header     9.683s  11.6%
drvinfo         9.667s  11.6%
close           9.650s  11.6%
seek_payload    9.650s  11.6%
mount           0.533s   0.6%
cd_image        0.367s   0.4%
cd_dir          0.350s   0.4%
cd_root         0.333s   0.4%
detect          0.000s   0.0%
```

- Interpretation: fixed per-file UCI command overhead dominates. The actual
  `LOAD_REU` transfer was only about 14% of measured loader time for this
  workload. The currently expensive per-file steps are open, file-info,
  read-header, seek, close, and the repeated identify/drive-info calls. The
  one-time path/mount/CD work is tiny after the launcher keeps the image
  mounted for the session.

## 2026-06-29 Fast-Raw Timing Variant

- A second probe variant tested the aggressive hot path: after the image is
  mounted, skip repeated detect/identify/drive-info and skip per-file
  `FILE_INFO`, PRG header read, and seek. Each file uses the known PRG file
  length from the build side, then does `OPEN`, one `LOAD_REU` from file offset
  0, and `CLOSE`. This deliberately loads the two-byte PRG address header into
  REU too; it is a timing experiment, not a valid launcher contract yet.
- C64U run `USB1/UTF201821.D81` completed 29/29 fast-raw DMA loads:
  `DONE:01`, `FAIL:00`, `SUCCESS:1D`, `loaded_kb=607`, `total_ms=33300`.
- Fast-raw stage timing for the 29 loads:

```text
load_reu       11.483s  34.5%
open           10.233s  30.7%
close           9.367s  28.1%
mount           0.517s   1.6%
cd_image        0.367s   1.1%
cd_dir          0.350s   1.1%
drvinfo         0.333s   1.0%
cd_root         0.317s   1.0%
identify        0.317s   1.0%
detect          0.000s   0.0%
file_info       0.000s   0.0%
read_header     0.000s   0.0%
seek_payload    0.000s   0.0%
```

- Interpretation: `LOAD_REU` is not a standalone filename-based operation in
  the command shape we are using; the file still has to be selected/opened
  first. So the practical minimal per-file hot path is `OPEN + LOAD_REU +
  CLOSE`, not just `LOAD_REU`. This cut total measured load time from 83.167s
  to 33.300s for the same 29-item workload, about a 2.5x improvement, before
  any future contract change to handle the two-byte PRG header correctly.

## 2026-07-04 Launcher UI And C64U Verification

- Current deployed manual-test artifact: `USB1/readyos-v0.2.5e-d81.d81`.
  The release-side config for this build uses
  `c64u_image_path=/usb1/readyos-v0.2.5e-d81.d81`, so the launcher and the
  automation are testing the same fresh-named D81 rather than a reused
  `readyos.d81` mount.
- The C64U automation deletes the same remote filename before upload, uploads
  by FTP, verifies the FTP listing, sets Drive A to 1581, clears all 256 REU
  banks with a tiny REST-run PRG, resets before mount, mounts the D81 as drive
  A with `type=d81`, resets again, and only then starts ReadyOS.
- The boot path used by the shell harness is not an autoload configuration
  path. After the hard reset it reaches BASIC and types `LOAD"BOOT",8` then
  `RUN`; app loading is triggered later by launcher UI actions.
- Do not poll REST screen/RAM during the active booter/launcher disk load. The
  harness waits through the boot window first, then captures screens. During
  app DMA loads it is safe to capture screen RAM because the C64 side is showing
  the launcher loading UI.
- The visible UCI breadcrumb was reduced to one stage digit on the bottom
  loading line. The working UI form is now:
  `DMA LOADING  1/1   -  9` for simple apps and
  `DMA LOADING  6/10  -  1` style progress for ReadyShell main plus overlays.
  The old second debug digit in the middle of the screen was removed.
- The bottom loading line explicitly says `DMA LOADING` or `DISK LOADING`.
  A hardware regression run should search for `DISK LOADING` in captured
  screens/logs when DMA is expected; none appeared in the verified `0.2.5E`
  runs below.

Measured launcher map for the verified `0.2.5E` build:

```text
LAUNCHER_DMA_LOAD=1 verified build:
  CODE   $1033..$B482 size $A450
  RODATA $B483..$BA6F
  DATA   $BA70..$BAA1
  INIT   $BAA2..$BABD
  ONCE   $BABE..$BAE3
  BSS    $BAE4..$C514 size $0A31
```

Verification on C64U, all from Terminal-owned/background shells:

- Static checks passed:
  `python3 build_support/verify_launcher_dma_gate.py`,
  `python3 build_support/verify_memory_map.py`,
  `python3 build_support/verify_resume_contract.py`,
  and `bash -n build_support/run_readyos_boot_c64u_rest.sh`.
- Enter on Editor passed:
  `READYOS_EDITOR_DIRECT_DMA_RETURN_PASS`. Captures showed
  `DMA LOADING  1/1   -  2`, then `- 7`, then `- 9`, Editor opened, STOP
  returned to the launcher, and the launcher showed `DMA:ON`.
- F3/load-selected Editor passed:
  `READYOS_EDITOR_LOAD_SELECTED_PASS`. The load-selected screen showed
  `DMA LOADING  1/1   -  9`, then `PRESS ANY KEY`; the launcher returned with
  `DMA:ON`, and Enter opened Editor from REU.
- Load-all plus ReadyShell overlays passed:
  `READYOS_LOADALL_READYSHELL_OVERLAY_PASS`. Captures showed ReadyShell
  overlay progress such as `DMA LOADING  6/10`, `7/10`, `8/10`, `9/10`, and
  `10/10`, then later simple apps as `1/1`. ReadyShell reached
  `READYOS READYSHELL` / `LOADING DONE` and passed `VER`, `LST "RSHELP"`,
  `CAT "RSHELP" . TOP 1`, and another `VER`.
- Browse/app-manifest load passed:
  `READYOS_MANIFEST_SIDETRIS_PASS`. The manifest browser selected Sidetris,
  the load screen showed `DMA LOADING  1/1   -  9`, the launcher returned with
  `DMA:ON`, and Sidetris launched.
- ReadyBASIC F3/load-selected passed:
  `READYOS_READYBASIC_LOAD_SELECTED_PASS`. The load-selected screen showed
  `DMA LOADING  1/1   -  9`, the launcher returned with `DMA:ON`, and
  ReadyBASIC reached its `READY.` prompt.

Current automation caveat:

- The harness can briefly look like it is still at BASIC after `LOAD"BOOT",8`
  because it intentionally sleeps through the boot window before capturing the
  launcher. Do not classify that as a launcher crash unless the later
  `launcher_wait` capture fails and the final screen/memory dump also show no
  ReadyOS progress.

## 2026-07-05 DMA Must Not Fall Through To Disk

The C64U manual test exposed a launcher integration bug that the earlier UI
checks were too weak to catch. The launcher showed `DMA:YES`, then a selected
app showed `DMA LOADING`, but after the Ultimate DOS attempt returned failure
the same load fell through to the normal disk/KERNAL loader. To a human this
looked like:

- Editor: `DMA LOADING`, then slow `DISK LOADING` for the same app.
- ReadyShell: each `1/10`, `2/10`, etc. overlay could show a DMA stage and
  then load the same file through disk, making the overlay preload extremely
  slow and ambiguous.

Correct launcher behavior:

- If DMA is not available at all, the normal disk/chunked loader remains the
  right fallback. This keeps regular VICE and non-Ultimate hardware working.
- If the launcher actually attempts an Ultimate DOS DMA load for the current
  app/resource, a failure must be a failure for that item. Do not continue into
  the disk path for the same item, and do not let `load all to REU` retry that
  same item through disk.
- ReadyShell resources must follow the same rule as the main app PRG. A failed
  DMA attempt for an overlay must not silently fall through to the chunked disk
  resource loader.
- The UI distinction matters: `DMA:YES` means configured/available, `DMA:ON`
  means at least one successful DMA transfer has completed, `DMA LOADING` means
  the current transfer is using the Ultimate DOS path, and `DISK LOADING` means
  the normal disk loader is active.

Implementation lesson:

- Keep one small launcher-local flag for "the current load attempted DMA".
  Clear it when starting `load_app_to_reu()`, set it immediately before calling
  the Ultimate DOS loader for either the main PRG or a resource PRG, and use it
  to suppress load-all's historical retry path after a DMA-attempted failure.
  This cost one BSS byte and kept the behavior contained inside the launcher.
- Do not mark an app/resource as loaded unless the DMA path completed and the
  expected REU/control metadata was updated. Showing a DMA progress digit is
  not proof that the app is usable.

Automation lesson:

- A quiet sleep followed by "did the app eventually load?" is not enough for
  this feature. It can pass while hiding the exact bug we care about.
- Hardware tests must capture the loading screen repeatedly and treat
  `DISK LOADING` as a failure whenever DMA is expected. They should also retain
  the screen text that caused the failure.
- The useful C64U checks are now:
  - Load all to REU plus ReadyShell overlay smoke.
  - Enter on a single-file app such as Editor.
  - F3/load-selected or equivalent background load for ReadyShell overlays.
  - Browse/app-manifest load, then launch the selected app.
- A post-run grep across captured screens is a good sanity check: successful
  DMA runs should have many `DMA LOADING` sightings and zero `DISK LOADING`
  sightings.

Verified C64U regression evidence for the corrected launcher:

```text
READYOS_LOADALL_READYSHELL_OVERLAY_PASS
READYOS_EDITOR_DIRECT_DMA_RETURN_PASS
READYOS_READYSHELL_OVERLAY_SMOKE_PASS
READYOS_MANIFEST_SIDETRIS_PASS
```

The captured hardware artifacts for that run contained 267 `DMA LOADING`
matches and 0 `DISK LOADING` matches across the load-all, Enter, F3/load-
selected, and manifest-load paths.

Fresh DMA launcher map after the fix:

```text
LAUNCHER_DMA_LOAD=1 verified build:
  CODE   $1033..$B49E size $A46C
  RODATA $B49F..$BA8B
  DATA   $BA8C..$BABD
  ONCE   $BADA..$BAFF
  BSS    $BB00..$C531 size $0A32
```
