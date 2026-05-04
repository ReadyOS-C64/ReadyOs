# precog (easyflash)

- Release Line: `0.2.1`
- Artifact Build: `0.2.1`
- Kind: `easyflash`

## Why This Variant Exists

- Full ReadyOS cartridge boot path for VICE and EasyFlash-capable Ultimate-family setups.
- Uses the cartridge for cold boot and preload, then runs the launcher and apps from REU-backed RAM.
- Keeps a companion `D64` online for runtime files and disk-backed app data instead of trying to force everything into the cartridge image.

## Artifacts

- Cartridge: `readyos_easyflash.crt`
- Companion Disk: `readyos_data.d64`

## What The D64 Is For

- `readyos_data.d64` is not the primary boot medium. The cartridge is.
- The `D64` stays mounted on drive `8` so ReadyOS still has a normal disk target for runtime files, help content, app data, and disk-backed workflows.
- Think of this SKU as `cartridge + companion drive 8 disk + REU`, not as a standalone cartridge-only image.

## Expected Boot Behavior

- Enable the REU with `16MB`.
- A long blue-background preload stage is normal on cold boot.
- The boot loader now uses border colors as a progress signal.
- light blue border: loader setup and general control flow
- green border: shim install and shared-state setup
- yellow border: copying payload from cartridge into RAM
- orange border: stashing staged RAM into REU, or restoring launcher state from REU
- light green border: final handoff into the launcher
- The border may alternate between yellow and orange repeatedly while apps and ReadyShell overlays are being preloaded. That means the machine is working, not frozen.

## VICE Setup

- Enable the REU with `16MB`.
- Attach `readyos_data.d64` to drive `8`.
- Attach `readyos_easyflash.crt` as an EasyFlash cartridge.
- If the cartridge has already started before the disk is mounted, reset after both are attached.

### VICE Command Example

```sh
x64sc -reu -reusize 16384 -cartcrt readyos_easyflash.crt -drive8type 1541 -devicebackend8 0 +busdevice8 -8 readyos_data.d64
```

## Boot

- Recommended mount order: mount `readyos_data.d64` on drive `8`, then attach `readyos_easyflash.crt`, then reset or power on.
- This variant does not use the disk-side `PREBOOT -> BOOT` chain. Resetting with the cartridge attached starts the EasyFlash loader directly.
- Keep the `D64` mounted after boot. The launcher and apps still expect drive `8` to exist for normal file and data access.

## C64 Ultimate / Ultimate 64

- Copy both `readyos_easyflash.crt` and `readyos_data.d64` to the target storage.
- Mount `readyos_data.d64` on drive `8`.
- Attach `readyos_easyflash.crt` as an EasyFlash cartridge image.
- Enable the REU and set it to `16MB`.
- Reset or cold boot the machine after both media are attached.
- If you attach the cartridge first and the disk later, reset once more so the full ReadyOS runtime sees the correct drive `8` companion disk from the start.
