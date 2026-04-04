# Failure Patterns

## Recurrent motifs observed in this repo
- Shim relocation and alignment drift caused hard REU failures in prior sessions.
- Contract drift around `$C800` jump table and reu_setup offsets can produce crashes to BASIC.
- VICE monitor transport instability can create false negatives (`E_SCREEN_READ_FAILED`, `E_DUMP_FAILED`).
- Runs may reach overlay milestones then fail during post-prompt capture (`E_UNEXPECTED`, partial dumps).

## Runtime error code hints
- `E_VICE_EXITED`: autostart chain or process lifecycle issue.
- `E_MONITOR_UNREACHABLE`: stale process/port ownership conflict.
- `E_SCREEN_READ_FAILED`: monitor short-read timeout, often transport-related.
- `E_READYSHELL_TIMEOUT`: likely overlay/preload phase stall.

## Discipline rules
- For CAL26 REL work: use `xrelchk` harness as source of truth.
- Do not use `src/apps/dizzy/dizzy.c` as REL open/position/read/write reference.
- Change one REL transport variable at a time, rerun probes, and log in `CLAUDEWORKING/CAL26_REL_PROGRESS.md`.

## Preferred triage order
1. Verify static memory contracts.
2. Verify map/headroom and overlay fit.
3. Analyze run manifest and state captures.
4. Correlate with recent failure motifs.
5. Escalate to live capture if evidence remains inconclusive.
