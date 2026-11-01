# Reported issues

**BUG-003 — EasyFlash direct app switch (0.5 RC2, 2026-10-31).** In VICE,
open Editor, type a marker, return with Ctrl+B, use ReadyShell, then restore
Editor from the launcher. F2 from that warm Editor produced a blank display
instead of ReadyShell; F4 did not visibly restore the marker. The same
sequence passed on regular D81. Launcher-mediated switching passed on both
media, so use Ctrl+B and select the next app as a workaround. No root cause or
physical-cartridge result has been established.

From the repository root, with `VICE_TASKS_ROOT` pointing at the shared VICE
harness, run on disposable media (the emulator may write to its disk):

```sh
repro=$(mktemp -d)
cp Releases/0.5/precog-easyflash/readyos_data.d64 Releases/0.5/precog-easyflash/readyos_easyflash.crt "$repro/"
python3 build_support/launcher_return_framework_plan.py --target easyflash \
  --disk "$repro/readyos_data.d64" --boot "$repro/readyos_easyflash.crt" \
  --direct-switch --out "$repro/switch.json"
dotnet run --project "$VICE_TASKS_ROOT/src/ViceTasks.Binary/ViceTasks.Binary.csproj" \
  -- run-plan --plan "$repro/switch.json" --close-vice --no-tui
```

For the control use `--target d81`, a copy of the regular D81, and its
PREBOOT PRG as `--boot`. Both expect ReadyShell after F2 and the Editor
marker after F4. Raw captures and machine-local run logs stay out of Git.

The dates below record when each issue was observed. Reproduce it on the intended release image before assigning a current SKU status.

| Date | BugID | Description |
| --- | --- | --- |
| 2026-04-09 | BUG-001 | `simplefiles`, at least in the D81 version, crashes sometimes on reentry. |
| 2026-04-09 | IMP-001 | Improvement: `editor` and `quicknotes`, and possibly `tasklist`, do not currently provide a clear way to create a new document/file from the app UI. |
| 2026-04-11 | BUG-002 | `readyshell` drive queries can become stale after live disk topology changes in VICE. `DRVI`/`LST` work correctly on initial attached drives and correctly fail on missing drives, but after detaching and reattaching a disk during the same session, the reattached drive may continue to report as missing until ReadyOS is restarted. |
