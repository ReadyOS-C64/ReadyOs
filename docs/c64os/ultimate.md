## Ultimate C64OS bridge: complete setup

This is the full Ultimate application set plus the C64OS bridge, standalone
Ultimate SETUP and Ultimate Zip. The bridge is the first main-catalog entry.
The stock Ultimate SKU remains separate. ReadyOS is already compiled with
**skip 39**; no custom build is needed for this reservation.

### Files in this release folder

- `readyos-v0.5-ultimate_1.d81`: ReadyOS boot, all apps, data, modules and media.
- `readyos-v0.5-ultimate_2.d81`: optional ReadyBASIC examples disk.
- `bridge.car`: native C64OS Bridge Setup and ReadyOS return utility installer.
- `bridge.json`: CAR checksum and return-utility provenance.
- Host-side PREBOOT/BOOT PRGs and these README/help documents.

Supply your own licensed **C64OS 1.09 installation**; its operating-system image
is not included. Use PAL, 1 MHz and an enabled **16 MiB REU** for the current
bridge contract. C64OS must boot and operate independently on its selected
storage before attempting interop. Physical CMD/SD2IEC paths have not received
the same validation as the IDE64 test environment.

### Install and prepare C64OS

1. Start C64OS independently. Import `bridge.car` with its File Manager/Installer,
   then boot C64OS again after Installer exits.
2. Open **Bridge Setup** in Applications. It verifies version 1.09 and recognizes
   only the original or supported patched REU library.
3. Select **8–32 banks** (0.5–2 MiB); **32 banks / 2 MiB** is the largest supported
   allocation. Apply / Update, confirm, and restart C64OS. All offered values
   require the same ReadyOS skip 39. Keep its library backup for recovery.
4. Confirm the installed **ReadyOS** utility is accessible through Utilities,
   or add it using C64OS Menu Editor. The CAR does not replace the global menu.

### Mount and boot ReadyOS

1. Copy disk 1 to `/usb1/C64OS/READYOS.D81` on the Ultimate. That matches this
   SKU's default DMA path `/usb1/c64os/readyos.d81`. If using another path/name,
   mount that image on drive A/device 8 and run `LOAD"SETUP",8,1` then `RUN`;
   use SETUP to select the actual image and save its DMA configuration.
2. Keep ReadyOS disk 1 on device 8. For a native 1581 C64OS installation, mount
   its own prepared system image on drive B/device 9. Leave ReadyOS's examples
   disk unmounted while device 9 is needed by C64OS. IDE64 installations can
   remain on their configured IDE64 device instead.
3. Fresh-reset the machine, then `LOAD"*",8` and `RUN`. Enable the 16 MiB REU
   before boot; do not substitute the stock skip-zero Ultimate boot image.
4. Select **c64os bridge**, then **Boot location / settings (L)**. Set the actual
   OS device, partition, directory and boot filename; for a device-9 system disk,
   start with device 9, partition 0, empty directory and `booter`, adjusting them
   to your installation. Save and verify the settings on device 8 as `c64os.cfg`.
   With IDE64 detected, unsaved defaults are device 12, partition 1, `//os`,
   `booter`; without IDE64 they are device 8/current directory/`booter`, so the
   location must be configured before a device-9 cold boot.
5. Choose **Start / resume C64OS (C)** and allow disk activity to finish.

### Switch and resume

In C64OS, open the installed **ReadyOS** utility through Utilities. It returns
to the suspended ReadyOS bridge. Use Ctrl+B to visit the launcher and another
ReadyOS app, then reopen the bridge and choose Start / Resume to continue the
same C64OS session. Location edits affect the next cold boot, not a parked
session. The RAM and missing-boot tests in the bridge are diagnostic actions.

Physical REU banks 0–31 are reserved for C64OS (only the selected cap is used),
32–38 for bridge snapshots/reserve, 39 for ReadyOS control, and 40–255 for ReadyOS
apps/resources. GDOS/GEOS bridge SKUs use a different layout; boot each SKU fresh
rather than exchanging boot images while a context is parked.

### Recovery and validation boundary

To remove the cap, run Bridge Setup's Undo and confirm. The next C64OS boot must
be independent of ReadyOS. After undo the original detector may touch the whole
REU. If a library write is interrupted, restore `library/bridge.rec` as
`library/rec.lib.o` before rebooting C64OS. Reapply a supported cap before using
this bridge again. Keep valuable files backed up and switch only when disk I/O
has completed; custom IRQ/music and active-transfer suspension are unsupported.

This SKU's build and packaging checks do not constitute a new physical hardware
round-trip test. Existing bridge tests and the user's manual tests cover prior
configurations. Ultimate SETUP/DMA acceptance requires real Ultimate hardware;
VICE cannot validate the Ultimate command interface. The native setup source,
CAR builder and switching transport are all included in this repository.
