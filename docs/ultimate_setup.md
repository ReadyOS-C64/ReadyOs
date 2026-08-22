# Ultimate D81 SETUP

`precog-ultimate` is the C64 Ultimate-first full-content D81 SKU. Its launcher
is compiled with Ultimate DOS DMA support, and its generated `apps.cfg` begins
with `dma_loading=1` and an empty `c64u_image_path`. The image also contains
`SETUP`, a standalone first-run utility that records the exact Ultimate host
path of that D81 before ReadyOS boots.

SETUP is deliberately not a ReadyOS launcher app. It is a normal `$0801` C64
program that links only the focused ReadyOS TUI micromodules needed for its
screen, window, menu, and small UI helpers. It has no ReadyOS shim, ReadyFS
architecture, or overlay dependency. ReadyFS was used only as a reference for
the proven Ultimate DOS command spellings and resilient UCI state machine.

## First Run

1. Copy the Ultimate SKU D81 into a permanent folder on `usb1` or the SD card.
2. Mount that same D81 on emulated drive `8`.
3. From BASIC, run `LOAD"SETUP",8,1`, then `RUN`.
4. Resolve any prerequisite marked `MISSING` or `OFF/ERROR`, then press `F5`.
5. Browse from `/` through the active Ultimate storage volumes and folders.
   SETUP displays folders and D81 files only.
6. Select the ReadyOS D81 and confirm the update.
7. After `CONFIGURED - READYOS DMA IS READY`, exit with RUN/STOP and reset or
   boot `PREBOOT` normally.

Do not move or rename the D81 after setup. If its host path changes, mount it on
drive `8` and run SETUP again.

## Controls

- cursor up/down: move selection
- RETURN: enter a folder or select a D81
- LEFT or DELETE: parent folder
- F1 / F3: previous / next page
- F5: retest REU, UCI, Ultimate DOS, and the saved path
- F7: reapply and verify the currently saved path; if no path is available,
  enter an absolute host D81 path directly
- RUN/STOP: exit

Pages contain 14 filtered entries. SETUP rescans the Ultimate DOS directory
stream for each page and still drains every response block, even after the
visible page is full.

Directory and path labels normalize host-ASCII letters and underscores for
the C64 display. This affects only the visible copy; navigation and Ultimate
DOS commands retain the exact filename bytes returned by the device.

The screen follows the normal ReadyOS app header language without enclosing
the whole display: a shallow light-blue 40-column title box contains separate
REU, UCI, and Ultimate DOS label and value rows, its closing rule ends the
header, and the current host path appears immediately below it. A blank row
separates the path from the browser. The browser, guidance, and notice areas
have no side or bottom border.

## Prerequisites and Failure Guidance

SETUP performs three independent checks:

- `REU`: a non-destructive stash/fetch probe that restores the tested bytes;
  16 MB is recommended, while ReadyOS requires at least 1 MB.
- `UCI`: the Ultimate Command Interface ID is probed at the supported register
  bases.
- `ULT.DOS`: target 1 must answer IDENTIFY with an Ultimate DOS identity.

If a check fails, enable the REU, Command Interface/UCI, and Ultimate DOS in the
Ultimate settings, then reset and run SETUP again. VICE has no Ultimate UCI
service and provides no useful acceptance coverage for this Ultimate-only SKU;
all SETUP UI and integration acceptance runs on physical Ultimate hardware.

SETUP initially reads `apps.cfg` from the mounted drive-8 image through IEC.
This is the one bootstrap boundary: Ultimate DOS does not expose a proven
pathless mapping from an already mounted IEC image back to its host pathname.
All host browsing, image mounting, path validation, and config mutation use
Ultimate DOS exclusively.

## Safe Config Commit

Selecting a D81 mounts it on drive `8`, enters the image through Ultimate DOS,
and requires a readable `apps.cfg` containing both `dma_loading` and
`c64u_image_path`. SETUP preserves all other bytes and values, changes those
two values, and then uses this checked commit:

1. create new `rdyset.seq` without replacing an existing file, preserving the
   final C64 SEQ type;
2. read it back and compare every byte;
3. rename Ultimate DOS name `apps.cfg.seq` to `rdyset.bak.seq`;
4. rename `rdyset.seq` to `apps.cfg.seq` (the C64 directory still shows
   `apps.cfg` with type SEQ);
5. read back and compare the committed file;
6. roll back from `rdyset.bak.seq` if commit verification fails;
7. remove the backup only after a verified commit.

Pre-existing staging names cause a safe failure; SETUP does not delete them or
unrelated files to make progress.

## Automation Contract

Host tests cover config parsing, growth/shrink rewrites, preservation, malformed
configs, and the UCI transaction state machine. Physical C64 Ultimate automation
covers both UI and integration behavior using fresh, uniquely named images under
owned `READYOS_SETUP_TEST` folders on `usb1` or the SD card; it never deletes or
renames unrelated storage content.

The physical acceptance matrix passed on 2026-08-22 at 1, 16, and 64 MHz. At
each speed it navigates into and back out of the `USB1` volume, rejects a missing
D81 path, performs the staged commit with a valid path, downloads the resulting
image, verifies the exact path and `dma_loading=1`, proves `apps.cfg` remains a
SEQ file, and proves no staging file remains. Separate physical UI cases disable
Command Interface/UCI and the REU in turn, verify SETUP's corrective guidance,
and restore the original Ultimate settings afterward.

A separate physical launcher matrix boots complete ReadyOS images in both path
states. A missing configured image must show `DMA:NO`, recommend SETUP, and load
an app through the normal disk fallback. An exact configured image must show
`DMA:YES`, omit the SETUP recommendation, and load the same app through Ultimate
DOS DMA; returning from that app must then show `DMA:ON`. Both fixtures live
under a fresh `USB1/READYOS_SETUP_TEST` folder.

The transport never uses CPU-speed delays as protocol pacing. It synchronizes
to quiet IDLE, handles asynchronous PUSH/ABORT, drains data and status queues,
observes the `DATA_MORE -> COMMAND_BUSY` transition next to `DATA_ACC`, and
waits for final quiet IDLE.

Relevant checks are:

```sh
make setup-host-tests setup-contract-check uci-protocol-check
python3 build_support/verify_launcher_dma_gate.py
python3 build_support/verify_release_directory_order.py --profile precog-ultimate
# Launch these only through their Terminal-owned wrappers:
/bin/bash build_support/start_setup_c64u_matrix.sh
/bin/bash build_support/start_setup_c64u_prereq_failures.sh
/bin/bash build_support/start_launcher_setup_states_c64u.sh
```
