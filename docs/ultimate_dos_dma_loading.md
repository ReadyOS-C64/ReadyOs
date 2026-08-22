# C64 Ultimate DOS DMA Loading

ReadyOS has a profile-gated launcher path that reads PRG payloads from a configured
C64 Ultimate disk image directly into their allocated REU locations through
the Ultimate Command Interface (UCI). This accelerates preload and cold app or
resource loading without changing the resident shim or the `$1000-$C5FF`
snapshot contract.

## Availability

- Compile gate: `LAUNCHER_DMA_LOAD`. The Makefile derives it from the selected
  profile: `precog-ultimate` sets it to `1`; portable profiles set it to `0`.
- Runtime gate: `dma_loading=1` in `apps.cfg`, plus a non-empty
  `c64u_image_path`. Both must be present before the launcher attempts DMA.
- The Ultimate SKU ships with DMA enabled but its image path empty, so the
  standalone SETUP utility can record the actual installed location safely.
- Launcher variant: regular disk launcher only; EasyFlash does not link this
  path because cartridge preload already supplies its REU payloads.
- Hardware target: C64 Ultimate/Ultimate-family UCI with Ultimate DOS support.
- Catalog keys: `dma_loading` and `c64u_image_path`.
- First-run guide: [`ultimate_setup.md`](ultimate_setup.md).

Build and run ReadyOS itself through the normal workflow:

```sh
/bin/bash ./run.sh --profile precog-ultimate --vice-fast
```

VICE does not provide the hardware UCI service. An enabled build detects that
condition and continues through the normal disk path.

## Runtime Flow

1. The launcher parses `dma_loading` and `c64u_image_path` from `apps.cfg`.
2. After drawing the launcher, it probes UCI and validates the image path.
3. For an app or resource load, Ultimate DOS mounts or reuses the configured
   image, obtains the exact PRG file size, skips the two-byte PRG load header,
   and issues an exact-size `LOAD_REU` into the loader-assigned bank and offset.
4. On success the launcher marks the allocation/resource loaded and the normal
   shim later restores app snapshots from REU.
5. If UCI is absent, the path is invalid, the image cannot be mounted, the file
   cannot be opened, or transfer verification fails, the launcher quiesces UCI
   state and falls back to its established KERNAL/disk loader.

DMA only changes how a cold PRG reaches REU. App switching still uses the same
resident shim, logical-token lookup in the ReadyOS bank at `$B740`, and `$B600` REU
stash/fetch operations.

## Launcher Indicator

- `DMA:YES`: UCI and the configured path are available, but no successful DMA
  load has yet been recorded.
- `DMA:ON`: at least one current or cached load used the DMA path.
- `DMA:NO`: DMA is disabled or unavailable; disk fallback remains active.

When `dma_loading=1` but the configured path is empty or malformed, UCI or
Ultimate DOS cannot be reached, or the configured directory/image cannot be
opened, the launcher also shows `RUN SETUP APP FOR FAST APP LOADING` on its
home screen. Normal drive loading remains available.

Transient `DMA LOADING n/n` progress text is used by hardware smoke tests.

## Exact-Size Rule

Ultimate DOS `LOAD_REU` must receive the payload's exact byte length (file size
minus the two-byte PRG header), not the capacity of the destination slot. This
is especially important for ReadyShell overlays sharing a resource bank: a
slot-sized transfer can consume bytes belonging to the following packed
payload. The loader also validates the expected PRG load address before
accepting the transfer.

## Verification

```sh
python3 build_support/verify_launcher_dma_gate.py
```

Focused protocol work is documented in
[`../probes/uci_dma/README.md`](../probes/uci_dma/README.md). Launcher acceptance
fixtures are in
[`../build_support/c64u_dma_acceptance/README.md`](../build_support/c64u_dma_acceptance/README.md).
The chronological hardware investigation remains in
[`../ULTIMATEDOS_DMA_LOADING_LESSONS_LEARNT.md`](../ULTIMATEDOS_DMA_LOADING_LESSONS_LEARNT.md).

C64 Ultimate REST/FTP automation must be launched from a separate,
long-running Terminal-owned/background shell as described in `AGENTS.md`;
foreground one-shot probes can report false connectivity failures.
