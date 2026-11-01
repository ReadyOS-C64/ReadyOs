# ReadyOS {{PUBLIC_VERSION}} release guide

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

{{PUBLIC_VARIANT_MATRIX}}

## ReadyShell diagnostic builds

{{DEBUG_VARIANT_NOTE}}

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


[Source and issues]({{GITHUB_URL}}) · [Homepage]({{MAIN_SITE_URL}}) · [Wiki]({{WIKI_URL}})
