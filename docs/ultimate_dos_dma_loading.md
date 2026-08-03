# C64 Ultimate DOS DMA Loading

ReadyOS has an opt-in launcher path that reads PRG payloads from a configured
C64 Ultimate disk image directly into their allocated REU locations through
the Ultimate Command Interface (UCI). This accelerates preload and cold app or
resource loading without changing the resident shim or the `$1000-$C7FF`
snapshot contract.

## Availability

- Build gate: `LAUNCHER_DMA_LOAD=1`.
- Default: disabled (`LAUNCHER_DMA_LOAD=0`). Normal release artifacts therefore
  use KERNAL/disk loading unless explicitly built with the gate enabled.
- Launcher variant: regular disk launcher only; EasyFlash does not link this
  path because cartridge preload already supplies its REU payloads.
- Hardware target: C64 Ultimate/Ultimate-family UCI with Ultimate DOS support.
- Catalog key: `c64u_image_path`, currently configured by
  `cfg/profiles/precog-d81.ini` as `/usb1/readyos.d81`.

Build and run ReadyOS itself through the normal workflow:

```sh
LAUNCHER_DMA_LOAD=1 /bin/bash ./run.sh --profile precog-d81 --vice-fast
```

VICE does not provide the hardware UCI service. An enabled build detects that
condition and continues through the normal disk path.

## Runtime Flow

1. The launcher parses `c64u_image_path` from `apps.cfg`.
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
resident shim, logical-token lookup in the ReadyOS bank at `$B940`, and `$B800` REU
stash/fetch operations.

## Launcher Indicator

- `DMA:YES`: UCI and the configured path are available, but no successful DMA
  load has yet been recorded.
- `DMA:ON`: at least one current or cached load used the DMA path.
- `DMA:NO`: DMA is unavailable; disk fallback remains active.

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
