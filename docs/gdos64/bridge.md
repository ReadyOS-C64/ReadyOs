## Experimental GDOS64 Bridge

The regular D81 includes GDOS64 Bridge in its main launcher catalog. The stock Ultimate
and other normal SKUs do not include it; `precog-ultimate-gdos` is a separate experiment. This is an experimental plain-C64 bridge for
the dedicated VICE lab, not a general hardware release.

The normal D81 skips **0 banks**, so switching is disabled. The experimental
configuration requires **16 MiB REU**, **ReadyOS skip 71**, and **GDOS64 English
1.03 build 260511 with 4 MiB DACC** on native C1581 drives. Do not use an external
C=REU RAMNative drive with this layout. Banks 0–63 belong to GDOS; 64–70 are
reserved for the bridge; bank 71 and above belong to ReadyOS. The current
prototype does not support other allocations, cartridges, active spooling,
music or running CIA timers.

Boot location defaults to device 9, current directory, `gd.boot`. Choose
**Boot location / settings** in the bridge to edit it; **S** saves and verifies
`gdos64.cfg` on device 8. Changes apply to the next cold boot. The return component
is the native **ReadyOS Bridge** desk accessory, built from `experiments/gdos64/accessory.s` and supplied as the companion
`gdos64-readyos-bridge.cvt` beside this release image. Install it with a native
GEOS Convert importer on your GDOS system disk. It does not
patch the GDOS kernel. Select it from GDOS's desk accessory menu to return.

Source checkout instructions, automation and memory details are in
`experiments/gdos64/README.md`. The private experiment disk is distinct from this
stock skip-0 release disk. Never enable switching merely by changing a runtime
catalog: ReadyOS's skip count is compiled into its boot image.
