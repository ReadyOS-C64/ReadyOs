# REU Registry V2 Working Notes

## Goal

- Keep the shim fixed at 512 bytes.
- Keep `$C600-$C7FF` as the hot resident bank-state path.
- Make logical REU bank `0` useful for a 64-app registry and loader-owned
  dependency/resource ownership.
- Keep launcher growth measured and boring.

## Accepted Shape

- `RCB0` schema is now version `2`.
- Bank `0:$0100` remains the 256-byte bank-type mirror.
- Bank `0:$0300` stores compact 64-entry app-state arrays copied from the
  launcher: logical bank, loaded flag, resource set, resource-loaded flag,
  drive, and default hotkey slot.
- Bank `0:$0500` stores larger app metadata, currently normalized PRG tokens.
- Bank `0:$0900` stores dependency/resource-bank arrays, currently the three
  per-app banks used by `rsovl` and `rbcore`.
- The registry writer lives in `src/lib/reu_control_registry.c` and is linked
  only by launcher builds. `reuviewer` keeps only the small header/bank-table
  mirror writer.

## Rejected Shape

- A formatted linked-list dependency writer was prototyped and rejected.
- Cost after first compile: launcher `4637 -> 2344` headroom, reuviewer
  `29670 -> 27712`.
- Splitting the writer removed the reuviewer blast radius, but launcher still
  paid too much for pretty records.
- The accepted array-copy shape measured launcher `4637 -> 3583` and reuviewer
  `29670 -> 29572`.

## Dependency Marker

- Known resource tokens may be suffixed with `+`, e.g. `rsovl+` or `rbcore+`.
- The following non-comment line after the description is consumed as a
  comma-separated dependency list.
- Host tooling validates dependency item syntax.
- The C64 launcher only requires the line to be non-empty; it does not retain
  the filename list in RAM yet.

## Open Risk

- Launcher loses `1054` bytes of app-window headroom in this checkpoint. This
  is acceptable only because no normal app pays it and the launcher still has
  `3583` bytes. Any future generic dependency loader must recover or justify
  its size.
