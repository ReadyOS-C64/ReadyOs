# App-Owned REU Allocation Notes

## Goal

Make `REU_APP_ALLOC` banks attributable in the global REU control bank so REU
Viewer can show app ownership and launcher unload can reclaim runtime banks
owned by the selected app.

## Implementation Shape

- Add a separate `reu_owned_alloc` micromodule instead of adding code to
  `reu_mgr_alloc.c`, so apps that do not need owner records do not pay for it.
- Current users:
  - QuickNotes: two note-storage banks tagged `note`, slots 1 and 2.
  - ReadyIRC: one scrollback bank tagged `scrl`, slot 1.
  - rirc-rrnet: one scrollback bank tagged `scrl`, slot 1.
- The micromodule writes a 16-byte rich resource record at `$0A00` with kind
  `REUCB_DEP_KIND_APP_ALLOC`.
- Allocation is strict: if the owner record cannot be written, the bank is
  freed immediately and allocation fails.
- Shim impact is expected to be zero.

## Verification Plan

- Captured before/after app headroom snapshots.
- Added and ran a focused regular D81 VICE probe:
  - load QuickNotes;
  - return to launcher so QuickNotes remains suspended with its two `U` banks;
  - open REU Viewer and navigate to one `U` bank to capture ownership text;
  - run a second clean boot inside the same wrapper, load QuickNotes, unload it
    from launcher, reopen REU Viewer, and capture that the former `U` bank is
    free;
  - dump `$C600-$C7FF` and sampled REU state for analysis.
- Run `make verify`.
- Run full regular SKU VICE automation.
- Run selected cartridge SKU checks for regression.

## Focused Probe Result

Passed:

```text
QUICKNOTES_OWNED_REU_SKIP_BUILD=1 QUICKNOTES_OWNED_REU_VISIBLE=0 QUICKNOTES_OWNED_REU_KEEP_VICE=0 build_support/run_quicknotes_owned_reu_probe.sh
QUICKNOTES_OWNED_REU_VISIBLE=0 QUICKNOTES_OWNED_REU_KEEP_VICE=0 make quicknotes-owned-reu-vice
```

Artifacts:

- before ownership run:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260607_173802`;
- after unload run:
  `/Users/karlprosserpp/dev/c64projects/readyosprecog/logs/vice_auto_20260607_173831`.

The before run asserted `$C623-$C624 == 03 03`, selected physical bank `$23`,
and REU Viewer displayed `APP ALLOC`, `OWNER: QUICKNOTES`, and `TAG NOTE`.

The after run loaded and unloaded QuickNotes, asserted `$C623-$C624 == 00 00`,
selected the former physical bank `$23`, and REU Viewer displayed `TYPE: FREE`.

## Regression Result

Passed:

```text
make verify
READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 make readybasic-vice-suites
LAUNCHER_REU_STATE_SKIP_BUILD=1 LAUNCHER_REU_STATE_VISIBLE=0 build_support/run_launcher_reu_state_probe.sh
READYSHELL_VISIBLE=0 build_support/run_readyshell_cross_app_resume_probe.sh
make easyflash-smoke
READYSHELL_VISIBLE=0 make easyflash-readyshell-vice-suites
git diff --check
```

Notes:

- The launcher REU-state probe had a test-only ambiguity: it waited for
  `READY.`, which could match the preboot READY line before ReadyBASIC was
  actually entered. The generated plan now waits for `readybasic` on first
  entry and then proves unload/reload with `RBSTABLE`.
- The cartridge pass used EasyFlash smoke plus the cartridge-translated
  ReadyShell flow. That flow also enters ReadyBASIC and returns before entering
  ReadyShell, then exercises `VER`, `LST`, and `CAT`.

## Headroom Delta

Before/after from `agentworking/reu_owned_alloc_headroom_before.json` and
`agentworking/reu_owned_alloc_headroom_after.json`:

| App | Headroom Before | Headroom After | Delta | Notes |
| --- | ---: | ---: | ---: | --- |
| launcher | 5387 | 5242 | -145 | frees owner-recorded app alloc records during unload |
| quicknotes | 9851 | 9164 | -687 | links `reu_owned_alloc` for two note banks |
| readyirc | 29339 | 28657 | -682 | links `reu_owned_alloc` for scrollback |
| rirc-rrnet | 18553 | 17871 | -682 | links `reu_owned_alloc` for scrollback |
| reuviewer | 29523 | 29454 | -69 | displays app alloc tag details |
| editor | 11768 | 11768 | 0 | no link impact |
| readybasic | 1090 | 1090 | 0 | no link impact |
| readyshell | 18185 | 18185 | 0 | no link impact |
