## Optional bridges on Ultimate disk 2

The stock Ultimate's second D81 contains ReadyBASIC examples plus optional
`c64os` and `gdos64` bridge PRGs. Their `app.c64os` and `app.gdos64` SEQ manifests
are placed near the top of that disk's directory, using the standard build
ordering rules. Each manifest selects its app on **device 9**. The regular
single-D81 SKU contains neither bridge nor their manifests/installer companions.

Keep stock Ultimate disk 1 on device 8 and disk 2 on device 9. In the launcher,
use **F5 (Browse)**, browse device 9, and select `app.c64os` or `app.gdos64`.
The added entry loads the corresponding PRG from device 9. Location settings
still save on device 8 as `c64os.cfg` / `gdos64.cfg`.

**This stock SKU still compiles skip zero. Both apps therefore display their
setup-required guard and refuse OS switching.** Loading an optional manifest
does not repartition the REU. Use the dedicated `precog-ultimate-c64os` (skip39)
or `precog-ultimate-gdos` (skip71) image for enabled switching, with its matching
native component and OS requirements. These are different fixed layouts, not a
single setting that enables both bridges together.

The stock release folder also supplies `bridge.car` (C64OS Setup/return utility)
and `gdos64-readyos-bridge.cvt` (GDOS return accessory), with checksum metadata.
They are companion host files, outside the D81 images. Follow the C64OS/GDOS
requirements and installation sections below before using the dedicated images.

Native OS storage may itself need device 9. The dedicated bridge SKUs keep their
active bridge on disk 1 and their examples disk optional; they do not inherit
these extra drive-9 bridge copies/manifests. Do not replace a native OS disk
with the examples disk while its suspended session still depends on that drive.
