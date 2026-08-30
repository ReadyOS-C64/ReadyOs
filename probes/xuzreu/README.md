# xuzreu

`xuzreu` is a diagnostic build of the real `uzip` ReadyOS application. It is
never launched directly. Physical automation boots the Ultimate D81 through
PREBOOT/BOOT/launcher with `runappfirst=uzip`, so `uzpk` and `uzwk` are
allocated and reclaimed by the authoritative ReadyOS ownership registry.

The cold run proves nonzero-offset LOAD_REU, short-at-EOF transfer counts,
bank-edge rejection, C64-side CRCs, SAVE_REU, close/reopen queue comparison,
and host byte interoperability. It frees `uzwk`, switches through REU Viewer,
warms back into uZIP without reseeding `uzpk`, unloads uZIP in the launcher,
and uses the already-loaded REU Viewer to prove the former package bank is
free.

Only the Terminal-owned `run-ultimate-plan` path on the physical C64 Ultimate
is functional authority. This probe has no emulator plan or target.

After a focused 16 MHz bring-up pass, the remaining required speeds can run in
one Terminal-owned sequence:

```sh
XUZREU_SPEEDS="1 64" /bin/bash build_support/start_xuzreu_c64u_matrix_terminal.sh
```

The matrix still creates a fresh compiled fixture root for every speed. It
never reuses or deletes an earlier root.
