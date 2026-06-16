# REU Bank 0 Lookup Working Notes

## Goal

Move shim-facing app snapshot token lookup into logical REU bank `0` and stop
showing false reserved app slots in the live REU allocation table.

## Decisions

- Keep the existing one-byte app token ABI for launcher, hotkeys, and shim
  calls.
- Token `0` remains direct launcher mapping: `physical = skip + 1`.
- Non-zero tokens are resolved by the shim from logical REU bank `0` offset
  `$2F00 + token`.
- `$C83D` is the shim scratch byte for the fetched physical bank.
- `$C83E-$C83F` remain reserved.
- Current lookup entries use the compact physical formula (`skip + 1 + token`)
  unless a specific bank-0 record overrides the token mapping. Token `1` can
  therefore resolve to `skip + 2`.
- Dynamic/resource scanning now starts at `skip + 2`.
- Clear shim bitmap bits no longer imply `REU_RESERVED`; old reserved entries
  collapse to `REU_FREE`.

## Headroom

Compared:

- before: `agentworking/reu_bank0_lookup_headroom_before.json`
- after: `agentworking/reu_bank0_lookup_headroom_after.json`

Changed app-window headroom:

| App | Before | After | Delta |
| --- | ---: | ---: | ---: |
| launcher | 5953 | 5841 | -112 |
| editor | 11730 | 11967 | +237 |
| quicknotes | 9536 | 9773 | +237 |
| calcplus | 6745 | 7170 | +425 |
| hexview | 32429 | 32854 | +425 |
| clipmgr | 13591 | 14016 | +425 |
| reuviewer | 28827 | 29010 | +183 |
| tasklist | 6087 | 6324 | +237 |
| cal26 | 8769 | 9006 | +237 |
| readyirc | 29024 | 29261 | +237 |
| rirc-rrnet | 18238 | 18475 | +237 |

Launcher pays the lookup-page writer/mirror cost. Split REU-manager app users
recover code because the old low-bank reserved/free path was removed.

## Verification

- `python3 build_support/verify_readyos_shim.py`
- `python3 build_support/verify_dynamic_launcher.py`
- `python3 build_support/verify_memory_map.py`
- `bash run.sh --build-all`
- `make verify`
- `READYSHELL_VISIBLE=0 build_support/run_readyshell_cross_app_resume_probe.sh`
- `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 make readybasic-vice-suites`
- `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 READYSHELL_VISIBLE=0 make easyflash-vice-suites`

Result: all passed. The EasyFlash plugin-command probe had one cold-start VICE
boot hang during preloader wait; killing the stuck emulator let the harness'
outer retry run the same plan again, and the retry passed. The combined
EasyFlash suite then completed the ReadyBASIC visual suite and ReadyShell
cross-app resume probe successfully.
