# ReadyOS 0.5 release guide

## Main editions

Choose the folder for your machine. Its README gives the exact filenames, required drives and app list.

| Setup | Release folder | First boot |
| --- | --- | --- |
| VICE or a 1581 drive | [Regular D81](precog-d81/README.md) | Enable the REU, mount the D81 on 8, then `LOAD"*",8` and `RUN`. |
| C64 Ultimate | [Ultimate dual D81](precog-ultimate/README.md) | Mount disk 1 on 8 and disk 2 on 9. Use `LOAD"SETUP",8,1` and `RUN` to configure fast loading before booting. |
| EasyFlash | [Cartridge and data disk](precog-easyflash/README.md) | Mount the data D64 on 8, attach the CRT and reset. |

For 1541, 1571 and Kung Fu Flash 2, use the media table below.

Allow at least 1 MiB for the portable disk editions. Use 16 MiB for the supplied EasyFlash setup. Dedicated bridges require 16 MiB.
On stock Ultimate, disk 2 holds 44 ReadyBASIC examples and the optional C64OS/GDOS launch manifests.
Stock Ultimate boots with skip zero. For switching, choose the dedicated [C64OS](precog-ultimate-c64os/README.md), [GDOS64](precog-ultimate-gdos/README.md) or [GEOS 2.0](precog-ultimate-geos20/README.md) package and install its native companion.

## Media variants

| Folder | Media | Best Fit | Why It Exists | Boot |
| --- | --- | --- | --- | --- |
| [`precog-d81`](precog-d81/README.md) | 1x `D81` on drive `8` | C64 Ultimate, VICE, or other 1581-capable setups; this is the recommended and default ReadyOS SKU. | Main full-content ReadyOS profile: one D81 holds the current app catalog, ReadyBASIC modules, and examples. | `PREBOOT -> BOOT` |
| [`precog-ultimate`](precog-ultimate/README.md) | 2x `D81` on drives `8` and `9` | C64 Ultimate users who want Ultimate DOS DMA loading and a guided first-run image-path setup utility. | Full Ultimate main app set on drive 8; ReadyBASIC examples and optional C64OS/GDOS apps/manifests on drive 9. Stock skip zero disables OS switching; dedicated bridge variants provide matched reservations. | `PREBOOT -> BOOT` |
| [`precog-easyflash`](precog-easyflash/README.md) | `CRT` cartridge plus companion `D64` on drive `8` | VICE and Ultimate-family setups that want cartridge cold boot with disk-backed runtime data. | Preloaded apps and ReadyShell overlays plus the required companion data disk. | `cartridge reset` |
| [`precog-dual-d71`](precog-dual-d71/README.md) | 2x `D71` boot set on drives `8` and `9`; 1x optional `D71` drive-9 swap | C64 Ultimate, Ultimate 64, or VICE setups using two 1571-class drives, with optional drive-9 disk swapping. | Two boot-time D71 images hold the core 1571 app set; a third optional drive-9 swap image adds lesser apps and the original 41 ReadyBASIC examples. | `PREBOOT -> SETD71 -> BOOT` |
| [`precog-kung-fu-flash-2-d81`](precog-kung-fu-flash-2-d81/README.md) | 1x `D81` on drive `8` | Kung Fu Flash 2 users who want one full-content D81 and the cartridge's 1MB REU mode instead of CRT cartridge mode. | Full-content single-D81 profile tuned for Kung Fu Flash 2 disk loading with a 1MB REU and no skipped REU banks. | `PREBOOT -> BOOT` |
| [`precog-dual-d64`](precog-dual-d64/README.md) | 2x `D64` on drives `8` and `9` | Real or emulated 1541-only setups that can mount two disks but not D71 or D81 media. | Reduced dual-disk profile for 1541-class environments that can mount two D64 images but not higher-capacity media. | `PREBOOT -> BOOT` |
| [`precog-solo-d64-readybasic`](precog-solo-d64-readybasic/README.md) | 1x `D64` on drive `8` | 1541-only users who want ReadyBASIC and the original graphics/sound examples on one disk. | Focused single-D64 ReadyBASIC profile with ReadyOS, ReadyBASIC, three sample packages and the original 41 examples; new media demos are on regular/Ultimate D81. | `PREBOOT -> BOOT` |
| [`precog-solo-d64-a`](precog-solo-d64-a/README.md) | 1x `D64` on drive `8` | THEC64, web emulators, or simple loaders that can mount only one D64 at a time. | Single-D64 subset focused on editor, reference, and dizzy for one-disk-only environments. | `PREBOOT -> BOOT` |
| [`precog-solo-d64-b`](precog-solo-d64-b/README.md) | 1x `D64` on drive `8` | THEC64, web emulators, or simple loaders that can mount only one D64 at a time. | Single-D64 productivity subset centered on quicknotes, clipboard, calculator, and files. | `PREBOOT -> BOOT` |
| [`precog-solo-d64-c`](precog-solo-d64-c/README.md) | 1x `D64` on drive `8` | THEC64, web emulators, or simple loaders that can mount only one D64 at a time. | Single-D64 planning subset centered on tasklist, calendar, and REU viewer. | `PREBOOT -> BOOT` |
| [`precog-solo-d64-d`](precog-solo-d64-d/README.md) | 1x `D64` on drive `8` | THEC64, web emulators, or simple loaders that can mount only one D64 at a time. | Single-D64 experimental subset for simple cells, calculator, 2048, and deminer. | `PREBOOT -> BOOT` |
| [`precog-solo-d64-e`](precog-solo-d64-e/README.md) | 1x `D64` on drive `8` | THEC64, web emulators, or simple loaders that can mount only one D64 at a time. | Single-D64 shell-focused subset for readyshell and its overlay payloads in one-disk-only environments. | `PREBOOT -> BOOT` |
| [`precog-geos20`](precog-geos20/README.md) | 1x `D81` on drive `8` | C64 Ultimate, VICE, or other 1581-capable setups; this is the recommended and default ReadyOS SKU. | Main full-content ReadyOS profile: one D81 holds the current app catalog, ReadyBASIC modules, and examples. | `PREBOOT -> BOOT` |
| [`precog-ultimate-c64os`](precog-ultimate-c64os/README.md) | 1x `D81` boot set on drives `8`; 1x optional `D81` drive-9 swap | C64 Ultimate users who want Ultimate DOS DMA loading and a guided first-run image-path setup utility. | Full Ultimate app set with the C64OS bridge, compiled REU skip 39, native return installer and setup guide. Drive 8 holds ReadyOS; the examples disk is optional while the target OS uses its own storage. | `PREBOOT -> BOOT` |
| [`precog-ultimate-gdos`](precog-ultimate-gdos/README.md) | 1x `D81` boot set on drives `8`; 1x optional `D81` drive-9 swap | C64 Ultimate users who want Ultimate DOS DMA loading and a guided first-run image-path setup utility. | Full Ultimate app set with the GDOS64 bridge, compiled REU skip 71, native return installer and setup guide. Drive 8 holds ReadyOS; the examples disk is optional while the target OS uses its own storage. | `PREBOOT -> BOOT` |
| [`precog-ultimate-geos20`](precog-ultimate-geos20/README.md) | 1x `D81` boot set on drives `8`; 1x optional `D81` drive-9 swap | C64 Ultimate users who want Ultimate DOS DMA loading and a guided first-run image-path setup utility. | Full Ultimate app set with the GEOS 2.0 bridge, compiled REU skip 71, native return installer and setup guide. Drive 8 holds ReadyOS; the examples disk is optional while the target OS uses its own storage. | `PREBOOT -> BOOT` |

## ReadyShell diagnostic builds

This release line also ships debug-trace variants for ReadyShell development: `precog-d81-rsdebug`, `precog-dual-d71-rsdebug`, `precog-solo-d64-e-rsdebug`. They are intended for debugging and instrumentation, not as the default end-user choice.

## Runtime And REU Contract

- The active app snapshot is `$1000-$C5FF` (`$B600` bytes). The resident 1 KB
  shim owns `$C600-$C9FF`; its public ABI remains at `$C800-$C9FF`.
- Physical REU bank `Skip` is the **ReadyOS bank** and is never allocated to an
  app/resource. Physical `Skip+1` is the first dynamic bank.
- Standard profiles use `reu_bank_skip=0`. The C64OS bridge reserves 39 banks.
  GDOS64 and GEOS 2.0 bridge profiles reserve 71. This setting is compiled
  into the boot/shim image; changing the disk's `apps.cfg` alone has no effect.
- The ReadyOS bank contains the launcher snapshot at `$0000-$B5FF` and
  schema-v5 mappings, status, clipboard, hotkeys, registry/catalog, audit, and
  launcher runtime state at `$B600-$FFFF`.
- Logical app tokens resolve through the explicit `$B740` table; they are not
  physical bank numbers and must not be converted with arithmetic.
- ReadyShell and ReadyBASIC resource banks are loader-assigned and recorded in
  the ReadyOS bank. ReadyBASIC retains its custom ca65/ld65 compact-image shape.


[Source and issues](https://github.com/ReadyOS-C64/ReadyOs) · [Homepage](https://readyos64.com) · [Wiki](https://readyos.notion.site)
