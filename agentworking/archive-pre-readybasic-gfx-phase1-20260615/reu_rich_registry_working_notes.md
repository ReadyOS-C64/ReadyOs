# REU Rich Registry Working Notes

## Objective

Move ReadyShell disk overlay placement out of launcher hard-coded tables and into
the app config / manifest dependency line while keeping:

- shim size unchanged;
- hot app registry arrays in REU bank 0 cheap to copy into shim-adjacent RAM;
- rich app/resource/file relationships in REU bank 0 for REU Viewer and future
  unload/audit tooling;
- ReadyShell/ReadyBASIC micromodule impact minimal;
- cartridge path allowed to remain generated/static, but represented in the
  same REU registry shape at runtime.

## Bank 0 layout decision

Keep the existing array-copy hot region:

- `$0300`: app snapshot bank / loaded / resource set / resource loaded / drive /
  default slot arrays.
- `$0500`: app filename array.
- `$0900`: per-app assigned resource-bank arrays.

Add a rich resource-entry array in the already reserved dependency area without
disturbing the hot arrays:

- `$0A00`: 64 compact resource/file records, 16 bytes each.

Record shape:

```text
0  app index
1  resource set
2  resource kind
3  physical REU bank
4  offset low
5  offset high
6  length low
7  length high
8  flags
9  next record index, 0xff if none
10 overlay/slot id
11 source drive
12-15 short name tag, first four chars
```

This is intentionally not a general parser AST. It is enough for REU Viewer to
show ownership and contents, enough for unload/audit to follow resource chains,
and small enough to update with simple DMA writes.

## Config syntax

Dependency line supports comma-separated entries:

```text
name@bank:offset
```

- `bank` is a resource-bank ordinal, not a physical bank: `0`, `1`, `2`, ...
- `offset` is hexadecimal or decimal within that 64K resource bank.
- Bare names remain accepted and use the ReadyShell default packing for
  compatibility with already committed catalogs.

Examples:

```text
rsparser@0:0000,rsvm@0:3800,rsdrvilst@0:7000,rsldv@1:0000
```

The C64 launcher only needs to parse this one compact line for `rsovl`.
ReadyBASIC can continue using the compact `rbcore,rbcode` line for now because
its resource banks are semantic banks rather than packed overlay slots.

## Risk controls

- Do not make normal apps link the rich registry writer.
- Keep the ReadyShell runtime reading the existing small `OV` metadata.
- Use config-driven placement in the disk launcher path, but leave EasyFlash
  generated/static placement intact while mirroring equivalent records.
- Add verifier checks for no remaining disk-launcher hard-coded ReadyShell
  overlay name/offset tables.
- Compare headroom before/after; expect launcher and REU Viewer to move, normal
  apps should not.

## Implementation result

- Bank 0 schema is now v3.
- `$0A00` rich records and `$0E00` dependency lines are implemented.
- Disk ReadyShell placement is read from config/manifest dependency lines.
- EasyFlash remains generated/static but emits the same v4 ReadyShell `OV`
  bank/offset metadata.
- REU Viewer can describe selected app/resource ownership from bank 0 metadata.
- Unload remains launcher-owned and clears rich records before freeing banks.

## Final headroom snapshot

Compared with `agentworking/reu_rich_registry_headroom_before.json`:

| App | Before | Final | Delta |
| --- | ---: | ---: | ---: |
| launcher | 3583 | 1009 | -2574 |
| reuviewer | 29572 | 28781 | -791 |
| readyshell | 18660 | 18327 | -333 |
| readybasic | 1029 | 1029 | 0 |

All other normal apps in the report are unchanged. The cost is concentrated in
launcher parser/loader/record ownership, REU Viewer display logic, and
ReadyShell's small dynamic `(bank, offset)` metadata consumer.

## Verification log

Passed:

- `python3 -m py_compile build_support/vice_easyflash_smoke.py`
- `python3 build_support/verify_reu_control_bank.py`
- `python3 build_support/verify_dynamic_launcher.py`
- `make readyshell-host-tests`
- `READYSHELL_VISIBLE=0 build_support/run_readyshell_cross_app_resume_probe.sh`
- `make easyflash-verify`
- `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 make readybasic-vice-suites`

Still to run before final commit:

- `make verify`

## Bugs found during verification

- `launcher_control_write_dep_line` erased dependency lines when the source was
  `launcher_dep_line_buf`. The function now copies first and clears only the
  tail, and static verification checks that this stays true.
- EasyFlash v4 metadata initially wrote offset high bytes into the low-byte
  position. The boot writer now emits `(bank, offset_lo, offset_hi)`.
- The EasyFlash smoke verifier initially compared only the first 16-byte
  monitor row at `$C760`. It now compares the assembled 36-byte region.
