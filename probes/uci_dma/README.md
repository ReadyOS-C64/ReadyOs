# UCI DMA Probe

Standalone C64 Ultimate probe for Ultimate DOS DMA loading from a mounted disk
image. This is the reference project for the technique; launcher integration is
intentionally out of scope until the D81 probe has been manually verified.

## Build

Build the D81 image:

```sh
PROBE_IMAGE_TYPE=d81 /bin/bash probes/uci_dma/build.sh
```

Output:

```text
build/uci_dma_probe/uci_dma_probe.d81
```

The image contains:

```text
probe
udma1
udma2
udma3
```

## Manual C64U Test

1. Copy `build/uci_dma_probe/uci_dma_probe.d81` to the C64U USB drive.
2. On the C64U, set drive A / device 8 to a 1581-compatible D81 drive.
3. Mount the D81 as drive 8.
4. At BASIC:

```basic
LOAD"PROBE",8
RUN
```

## Expected Success Screen

The probe should reach:

```text
PROBE DONE
P:42 C:51 F1:55 F2:55 F3:55
```

The screen should also show UCI found and a real SoftIEC bus byte.

`P` is the launcher-style plain-name check before the probe changes the
Ultimate DOS current directory. `42` means Ultimate DOS did not resolve
`udma1` as a valid file stat from the plain mounted-drive context. This is the
important negative proof for launcher integration: IEC drive 8 can have the D81
mounted while Ultimate DOS still cannot open plain filenames from that image
until its own directory context is changed.

`C` is the same pre-CD check after `COPY_UI_PATH`. `51` means the
`COPY_UI_PATH` command itself did not return a usable success status in the
REST-mounted case. So, for automation-mounted D81s, `COPY_UI_PATH` is not a
safe pathless bridge either.

`F1`, `F2`, and `F3` correspond to the three payload files:

```text
udma1 -> $11 pattern
udma2 -> $42 pattern
udma3 -> $83 pattern
```

Each `55` means the file was opened, read/seeked, DMA-loaded to REU, fetched
back, and verified after the probe has changed Ultimate DOS into the D81 image
filesystem.

## Automated C64U REST Test

The REST harness rebuilds, uploads, configures/mounts, resets cleanly, runs the
probe, captures screen/result memory, and validates the REU snapshots.

```sh
PROBE_IMAGE_TYPE=d81 C64U_HOST=10.0.0.79 C64U_REMOTE_DIR=USB1 \
  /bin/bash build_support/run_uci_dma_probe_c64u_rest.sh
```

The automation intentionally types one byte at a time and waits for the C64
keyboard buffer to drain. Bulk keyboard-buffer injection was not reliable
enough for this test.

Do not poll screen/RAM aggressively while KERNAL disk I/O is active. The
ReadyOS boot automation reproduced a stuck `LOADING LAUNCHER...` screen when it
captured memory during the booter's `LOAD "launcher",8`; adding a quiet wait
before the first REST memory read allowed the same normal D81 to boot and launch
Editor. Treat mid-load REST captures as intrusive.

## Automated VICE/dotnet Sanity Test

VICE does not provide the C64U UCI device, so this is a no-UCI regression check
rather than proof of Ultimate DOS DMA. It verifies that the D81 builds, mounts,
loads `probe`, reaches `PROBE DONE`, and leaves the DMA file-result blocks
untouched when UCI is absent.

```sh
/bin/bash build_support/run_uci_dma_probe_vice.sh
```

The runner uses the existing dotnet VICE task framework and validates the
result artifacts with:

```sh
python3 build_support/analyze_uci_dma_probe_run.py <run-dir> \
  --expect no-uci --version 41
```

## Important Boundaries

- This probe uses Ultimate DOS UCI commands for the DMA load path.
- SoftIEC is detected and reported, but it is not the DMA transport.
- The payloads are loaded only after Ultimate DOS has changed into the D81/D64
  filesystem context.
- The probe currently proves that "plain file from IEC drive 8's mounted image"
  is not the same context as "plain file from Ultimate DOS current directory."
- The probe also proves that REST-mounted D81 plus `COPY_UI_PATH` did not
  produce a usable Ultimate DOS image context.
- Do not use this probe as a launcher implementation directly; first manually
  verify the D81, then port the exact proven sequence.
