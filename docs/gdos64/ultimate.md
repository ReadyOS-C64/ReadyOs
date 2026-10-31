## Ultimate GDOS64 bridge: complete setup

This dedicated SKU contains the full Ultimate app set, SETUP, Ultimate Zip and
the GDOS64 bridge as its first launcher entry. It is already compiled with
**skip 71**. Stock Ultimate instead carries disabled optional bridge copies on disk 2 and uses skip zero.

### Files and requirements

Disk 1 (`readyos-v0.5-ultimate_1.d81`) holds ReadyOS and all apps, data, modules
and media. Disk 2 (`readyos-v0.5-ultimate_2.d81`) is optional ReadyBASIC examples.
The companion `gdos64-readyos-bridge.cvt` is the native return desk accessory;
`gdos64-bridge.json` records its checksum. Supply your own GDOS system disk.

The supported fixed contract is PAL, 1 MHz, enabled **16 MiB REU**, two native
C1581 drives, **GDOS64 English 1.03 build 260511**, and **4 MiB DACC**. Exclude
external C=REU RAMNative drives, cartridges, active spooling/music and running
CIA timers. Avoid SID mappings conflicting with the REU's DF00 address range.

### Install and boot

1. Boot GDOS independently and confirm its version, DACC size and disk operation.
   Import `gdos64-readyos-bridge.cvt` with a GEOS/GDOS-compatible Convert importer
   onto its system disk. Confirm **ReadyOS Bridge** is available as a native desk
   accessory. This installs our accessory; it does not patch the GDOS kernel.
2. Copy ReadyOS disk 1 to `/usb1/GD/READYOS.D81`, matching the default DMA path
   `/usb1/gd/readyos.d81`. Mount it on drive A/device 8. If using another host
   path, run `LOAD"SETUP",8,1`, then `RUN`, and select/save the actual image path.
3. Mount your prepared GDOS system disk on drive B/device 9. Keep ReadyOS disk 2
   unmounted while GDOS needs that drive. Enable the REU, fresh-reset, then boot
   ReadyOS using `LOAD"*",8` and `RUN`.
4. Open **gdos64 bridge**, then **Boot location / settings**. Defaults are device
   9, partition 0, empty directory and `gd.boot`. Adjust to your installation;
   save and verify `gdos64.cfg` on device 8. Location changes affect the next cold
   boot, not an existing suspended GDOS session.
5. Choose Start / Resume and let the native disk load visibly finish. At 1 MHz
   a black interval during loading is not proof of failure.

### Return, resume and memory ownership

In GDOS use **GEOS → Applications → DeskAccessories → ReadyOS Bridge** and
confirm. Once back in ReadyOS, you can use another app through Ctrl+B. Reopen
the bridge and choose Start / Resume to return to the parked GDOS session.
The confirmation is part of this accessory's UI. Switch only when I/O is idle.

REU banks 0–63 belong to GDOS's 4 MiB DACC; 64–70 are reserved for the bridge;
71 holds ReadyOS control; 72–255 hold ReadyOS apps/resources. Do not change the
DACC size or replace this boot image with skip-zero/skip-39 images while using
the bridge. A different allocation needs a corresponding implementation change,
not just a catalog edit.

### Recovery and validation boundary

If a version, memory or native-driver check refuses switching, correct the
configuration and boot fresh. The bridge's boot-failure test intentionally uses
an invalid boot target; it is a diagnostic, not the normal launch action.
Keep documents backed up, and do not rely on parked RAM surviving a reset.
If normal cold boot fails, verify GDOS independently before retrying interop.

Earlier VICE tests verify live geoWrite restoration, continued input and REU
isolation, and the user reports manual hardware testing. Rebuilding this SKU
checks packaging; it does not add a new physical round-trip qualification.
Ultimate-specific SETUP/DMA acceptance needs physical hardware. The accessory
source and CVT builder are checked in with the ReadyOS bridge transport.
