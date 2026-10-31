# C64OS Bridge Setup

Native C64OS Toolkit application for the ReadyOS bridge. Requires **C64OS 1.09**.
The app validates `settings/version.t` and the complete 217-byte `rec.lib.o`
against a known CRC32 before applying or undoing its six-byte bank-limit patch.
It uses the running OS's system file reference, not a hardcoded device/path.

The native cycle selector offers 8–32 banks, showing exact binary MB. Apply,
update and undo require explicit confirmation in the app and a later restart.
A full original is saved as `library/bridge.rec`; each write is read back, and
normal write failures attempt rollback. Fast App Switching thaw revalidates the
version and current on-disk limit. The running REU allocator is never resized.

The bridge's fixed layout requires a 16MB REU and ReadyOS `reu_bank_skip=39`
for **every** offered cap. Snapshot/reserved banks remain 32–38. Normal ReadyOS
D81 releases keep skip zero and refuse to switch until appropriately rebuilt.
Undo restores the original probe; the next C64OS boot must be standalone.

## Build

```sh
python3 experiments/c64os/setup/tools/build.py --readyos .
```

The ReadyOS normal D81 build invokes this builder from this checked-in source through
`build_support/build_c64os_car.py` after assembling its shared native return
utility. The resulting **install-type CAR v3** installs:

- `applications/Bridge Setup/` (main, menu, icon, About and Help)
- `utilities/ReadyOS`

It includes no licensed C64OS library or replacement global Utilities menu.
Open the return utility in the native Utilities browser or add it with Menu
Editor. ReadyOS copies the companion `bridge.car` to its D81 release folder.

## Tests

`tools/test_patch.py` executes the assembled 6502 implementation using py65.
It checks the original, all 25 caps, undo normalization, every single-byte
corruption, invalid sizes/caps/versions, actual assembled commit/rollback paths,
and the native `getsfref` destination-page contract. OS disk transport alone is
stubbed in those tests; licensed baseline data is read from the private sibling
experiment and is never included in output packages.

`tools/test_native_ui.py` installs the CAR via C64OS Installer in the explicitly
owned `bridge-setup-test-20260926` VICE environment, tests actual Toolkit mouse
selection, applies 8 banks, updates to 9, undoes, then reapplies 32. It reopens
through the real File Manager/Fast App Switching path and verifies the app's
native readbacks. Evidence is kept in that private test environment. The host
staging helper must place the CAR at the copied HDD's root first.

See `../readyosprecog/docs/c64os/bridge.md` for full user setup and recovery.
