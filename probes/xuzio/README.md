# xuzio

`xuzio` is the standalone physical-C64 authority for the exact UCI and
Ultimate DOS sources linked by Ultimate ZIP. It has no emulator runner.

The build requires a fresh run identifier:

```sh
XUZIO_RUN_ID=XUZIO-20260822-001 /bin/bash probes/xuzio/build.sh
```

Before launch, the physical runner must create only
`USB1/READYOS_UZIP_TEST/<run-id>` and place a file named
`.READYOS-UZIP-OWNER` there whose exact contents are `<run-id>`. The probe
refuses every mutation until it has opened and matched that marker through the
production Ultimate DOS queue path.

Acceptance is only through a Terminal-owned `run-ultimate-plan` session on a
physical C64 Ultimate at 1, 16, and 64 MHz. Each run root and its output files
are preserved for host-oracle inspection by default.

Start the full physical matrix from a Terminal-owned shell with:

```sh
/bin/bash build_support/start_xuzio_c64u_terminal.sh
```

The runner never mounts an emulator, never deletes a remote path, and rejects
an already-existing run identifier before uploading its ownership marker.
