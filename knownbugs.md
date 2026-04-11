| Date | BugID | Description |
| --- | --- | --- |
| 2026-04-09 | BUG-001 | `simplefiles`, at least in the D81 version, crashes sometimes on reentry. |
| 2026-04-09 | IMP-001 | Improvement: `editor` and `quicknotes`, and possibly `tasklist`, do not currently provide a clear way to create a new document/file from the app UI. |
| 2026-04-11 | BUG-002 | `readyshell` drive queries can become stale after live disk topology changes in VICE. `DRVI`/`LST` work correctly on initial attached drives and correctly fail on missing drives, but after detaching and reattaching a disk during the same session, the reattached drive may continue to report as missing until ReadyOS is restarted. |
