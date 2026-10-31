## Ultimate GEOS 2.0 bridge: complete setup

This dedicated SKU contains the full Ultimate app set, SETUP, Ultimate Zip and
**geos 2.0 bridge** as its first launcher entry. ReadyOS is already compiled with
**skip 71**. Stock Ultimate remains a separate skip-zero SKU without enabled GEOS switching.

### Files and requirements

`readyos-v0.5-ultimate_1.d81` holds ReadyOS and all apps/data/modules/media.
`readyos-v0.5-ultimate_2.d81` is optional ReadyBASIC examples. Install the companion
`geos20-readyos-bridge.cvt` on your GEOS system disk; `geos20-bridge.json` records
its checksum. The licensed operating system and core apps such as geoWrite are
not distributed in this package; use your own installation.

Use PAL, 1 MHz, an enabled **16 MiB REU**, two native C1581 drives, and **stock
GEOS 2.0** with its **512 KiB/eight-bank** REU allocation. Extended REU drivers
and other GEOS versions are outside this bridge contract. The low 4 MiB
reservation below is conservative protection, not a GEOS expansion patch.

### Install and boot

1. Boot GEOS independently and verify its disk/input operation. Use a native
   Convert importer to install `geos20-readyos-bridge.cvt` onto the system disk,
   where it becomes the **ReadyOS Bridge** desk accessory. Use the GEOS2 CVT,
   not the GDOS CVT: their version checks and session signatures differ.
2. Copy ReadyOS disk 1 to `/usb1/GEOS20/READYOS.D81`, matching this SKU's default
   DMA path `/usb1/geos20/readyos.d81`. Mount it on drive A/device 8. For another
   host path, run `LOAD"SETUP",8,1`, then `RUN`, and select/save the actual image.
3. Mount the prepared GEOS system disk on drive B/device 9. Do not mount the
   optional ReadyOS examples disk there while GEOS needs it. Enable the REU,
   fresh-reset, then `LOAD"*",8` and `RUN` to boot ReadyOS.
4. Open **geos 2.0 bridge**, then its boot-location settings. Defaults are device
   9, partition 0, empty directory and `geoboot`. Adjust them to your installation
   and save/verify `geos20.cfg` on device 8. The CBMFiles GEOS2 system image used
   in the lab has this GEOboot loader; other installations must provide the
   compatible native loader expected by this bridge.
5. Start / Resume GEOS and allow loading to visibly finish before interacting.

### Return, resume and memory ownership

Open **ReadyOS Bridge** from the GEOS desk-accessory menu and confirm the return.
It uses native accessory save/restore to preserve a displaced application such
as geoWrite. In ReadyOS use Ctrl+B for another app, then reopen the bridge and
Start / Resume to continue GEOS. Saved location changes apply to the next cold
boot, not to an already parked session. Switch only after disk I/O finishes.

REU banks 0–63 are reserved for GEOS, including native swap/reboot storage; 64–70
are bridge reserve/snapshots; 71 is ReadyOS control; 72–255 are ReadyOS apps and
resources. Do not mix this image with stock skip-zero or C64OS skip-39 boots.
Do not install a larger GEOS REU driver and assume it respects this reservation.

### Recovery and validation boundary

If the accessory rejects the version, allocation or driver setup, use the stock
supported setup and reboot. To diagnose a failed cold boot, first prove GEOS can
boot independently. Reset discards parked contexts; save documents to disk before
resetting. No GEOS kernel patch is installed by this accessory.

The prior 13-check VICE run includes cold/warm switching, live geoWrite text,
continued typing and reciprocal REU preservation. The user reports manual
hardware testing. These rebuilt packages have not received a new physical
round-trip run; Ultimate SETUP/DMA requires physical Ultimate acceptance. All
native accessory, transport and CVT build sources are included in this repo.
