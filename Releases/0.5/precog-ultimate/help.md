# precog ultimate (d81)

- Release Line: `0.5`
- Artifact Build: `0.5`
- Kind: `ultimate`

## Why This Variant Exists

- C64 Ultimate D81 with DMA loading enabled in apps.cfg and a standalone SETUP browser for locating and validating the image through Ultimate DOS.

## Artifacts

- Boot-time drive 8: `readyos-v0.5-ultimate.d81`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-preboot.prg`
- Host-Side Boot PRG: `readyos-v0.5-ultimate-boot.prg`

## Included Apps

- Drive 8: `editor` - editor
- Drive 8: `readyshell` - readyshell (beta)
- Drive 8: `simplefiles` - simple files
- Drive 8: `uzip` - ultimate zip
- Drive 8: `clipmgr` - clipboard
- Drive 8: `readybasic` - ready basic (beta)
- Drive 8: `cal26` - calendar 26
- Drive 8: `tasklist` - task list
- Drive 8: `reuviewer` - reu viewer
- Drive 8: `sysinfo` - system info
- Drive 8: `quicknotes` - quicknotes
- Drive 8: `calcplus` - calc plus
- Drive 8: `hexview` - hex viewer
- Drive 8: `simplecells` - simple cells (alpha)
- Drive 8: `game2048` - 2048 game
- Drive 8: `deminer` - deminer
- Drive 8: `dizzy` - dizzy kanban
- Drive 8: `readyirc` - readyirc
- Drive 8: `readme` - read.me
- ReadyBASIC is accompanied by all three external `rbm.*` module packages and the complete 41-program procedure, graphics, and sound example/test set.
- ReadyBASIC's banked `rbcore`/`rbcode` resources are carried inside the `readybasic` executable rather than as separate disk files.

## Disk Directory Order

- Each image uses the groups it needs in this order: boot chain; configs; ordinary SEQ/USR data; main app PRGs; overlays/modules; REL data; ReadyBASIC examples.
- On bootable images, `PREBOOT` is the first directory entry, followed by any `SETD71` / `SHOWCFG`, then `BOOT` and `LAUNCHER`, so `LOAD"*",8` selects the bootstrap.
- ReadyShell overlay PRGs and ReadyBASIC `rbm.*` module packages stay in the overlay/module group even though their file types differ.
- Images that do not carry a category simply omit it without changing the relative order of the remaining categories.

## Validation Target

- This is an Ultimate-only SKU. VICE does not provide the Ultimate UCI/Ultimate DOS services SETUP requires, so VICE testing has no acceptance value for this variant.
- SETUP and DMA acceptance run on physical C64 Ultimate hardware at 1, 16, and 64 MHz.

## Boot

- This profile uses the direct boot chain `PREBOOT -> BOOT`.
- There is no `SETD71` stage for this variant.
- Attach the single disk on drive `8`, then autostart `readyos-v0.5-ultimate-preboot.prg` or run `LOAD "PREBOOT",8` then `RUN`.

## C64 Ultimate

- Copy the listed disk image files to the target storage.
- Enable the REU with at least `1MB`; use `8MB` or `16MB` where available.
- The host-side boot PRGs are optional convenience files for emulator launching; the disk-side `PREBOOT` entry is the standard hardware boot path.
- This SKU compiles the regular launcher with Ultimate DOS DMA support and ships `apps.cfg` with `dma_loading=1`; disk fallback remains active whenever DMA is unavailable.
- Before the first ReadyOS boot, mount the D81 on drive `8`, run `LOAD"SETUP",8,1`, then `RUN`.
- SETUP is a standalone utility built from focused ReadyOS TUI micromodules. It checks REU, UCI, and Ultimate DOS, browses active Ultimate storage volumes/folders for D81 images, mounts the selection, validates its `apps.cfg`, and stages the exact host path into that image.
- SETUP uses F1/F3 for pages, cursor keys for selection, RETURN to enter/select, LEFT or DELETE to go to the parent, F5 to retest prerequisites, and F7 to apply a saved path or enter an absolute D81 path when none is available.
- After SETUP reports `CONFIGURED`, exit with RUN/STOP and reset or boot `PREBOOT`. Do not rename or move the D81 afterward without running SETUP again.
- Attach the single disk image on drive `8`, then boot with `LOAD "PREBOOT",8` and `RUN`.
- This variant boots directly from `PREBOOT` into `BOOT` and does not use `SETD71`.
