# REU Stale Last-Saved Bank Fix Notes

## Hypothesis

Manual unload/reload testing can leave `$C835` (`SHIM_LAST_SAVED`) pointing at a
logical app bank that the launcher has intentionally unloaded. A later launcher
sync can treat that stale hint as authoritative and re-set the low shim bitmap,
making a freed snapshot bank switchable again.

ReadyBASIC is especially sensitive because a stale warm snapshot can use old
resource-bank state before the dynamic bank resolver has refreshed it.

## Fix Shape

- Checkpointed prior non-binary/non-release delta first in `71d11a8`.
- No shim ABI or app changes.
- Launcher clears `$C835` when freeing the same snapshot bank.
- Launcher only heals `$C836-$C838` from `$C835` when that logical bank is still
  assigned to a current catalog entry.
- `build_support/verify_dynamic_launcher.py` has a static guard for this
  launcher-only invariant.

## Memory Discipline

- Before snapshot: `agentworking/reu_stale_state_headroom_before.json`
- After snapshot: `agentworking/reu_stale_state_headroom_after.json`
- Impact:
  - launcher headroom: `5413 -> 5386` (`-27`)
  - launcher CODE: `+27`
  - launcher BSS/heap: unchanged
  - ReadyBASIC, ReadyShell, REU Viewer: unchanged
- Shim size and ABI: unchanged.

## Verification

Completed:

- `make verify`: passed after implementation and again after the VICE runs.
- `LAUNCHER_REU_VISIBLE=0 LAUNCHER_REU_KEEP_VICE=0 build_support/run_launcher_reu_state_probe.sh`: passed.
  - run dir: `logs/vice_auto_20260607_142822`
  - after ReadyBASIC exit: `$C834-$C83F = 00 ff 02 00 00 08 00 20 00 22 00 00`
  - after unload: `$C834-$C83F = 00 ff 00 00 00 08 00 20 00 22 00 00`
  - after reload: `$C834-$C83F = 00 ff 02 00 00 08 00 20 00 22 00 00`
  - final in ReadyBASIC: `$C834-$C83F = 01 ff 02 00 00 08 00 20 01 22 00 00`
  - This proves unload clears `$C835` to `$FF` and clears the low shim bitmap,
    then reload rebuilds ownership from the real load path.
- `READYBASIC_VISIBLE=0 READYBASIC_KEEP_VICE=0 make readybasic-vice-suites`: passed on clean aggregate rerun.
  - An earlier aggregate run had one `readybasic_state_probe` timeout with
    `EXIT` typed on the ReadyBASIC screen; a focused rerun and the full aggregate
    rerun both passed, so this was treated as automation timing rather than the
    stale-bank bug.
- `READYSHELL_VISIBLE=0 build_support/run_readyshell_cross_app_resume_probe.sh`: passed.
  - run dir: `logs/vice_auto_20260607_151935`
  - covered Editor, ReadyBASIC, ReadyShell, `VER`, `LST`, and `CAT`.
