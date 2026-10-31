## Experimental GEOS 2.0 Bridge

The `precog-geos20` and `precog-ultimate-geos20` profiles are dedicated bridge
experiments. They use 16 MiB REU, ReadyOS skip 71, native C1581 drives, and stock
GEOS 2.0 with its eight-bank/512 KiB allocation. They are separate from stock
skip-zero releases. Banks 0–63 are reserved for GEOS, 64–70 for the bridge,
71 for ReadyOS control and 72–255 for ReadyOS apps. Other GEOS REU expansions
are not supported by this fixed layout.

The ReadyOS app is `geos20`. Boot location defaults to `GEOboot` on device 9;
its settings screen edits and saves `geos20.cfg` on device 8. Changes affect the
next cold boot. Install the companion `geos20-readyos-bridge.cvt` on your GEOS
system disk with a native Convert importer; it becomes the ReadyOS Bridge desk
accessory. Confirm its dialog to return to ReadyOS, then Start / Resume to return
to the suspended GEOS application. Native accessory sources are checked in at
`experiments/geos20/accessory.s`. GEOS kernel binaries are not patched.

The regular D81-derived GEOS experiment also carries the optional C64OS app and
its CAR, inherited from the base profile. Its compiled skip 71 does not satisfy
the C64OS bridge's skip-39 requirement, so C64OS switching is disabled there.
Use the matching compiled layout for the bridge you intend to run; one fixed
experimental image is not a universal multi-OS allocator.
