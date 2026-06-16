# REU Refactor Headroom Deltas

## Baseline Bank 0 Mirror Delta

Compared the pre-refactor baseline to commit `e7b5487 Add REU control bank
mirror baseline`.

| App | Before headroom | After headroom | Change | Runtime end before -> after |
|---|---:|---:|---:|---|
| launcher | 12643 | 11122 | -1521 | $949C -> $9A8D |
| editor | 11665 | 11665 | 0 | $986E -> $986E |
| quicknotes | 9471 | 9471 | 0 | $A100 -> $A100 |
| calcplus | 6593 | 6593 | 0 | $AC3E -> $AC3E |
| hexview | 32277 | 32277 | 0 | $47EA -> $47EA |
| clipmgr | 13439 | 13439 | 0 | $9180 -> $9180 |
| reuviewer | 31611 | 29372 | -2239 | $4A84 -> $5343 |
| sysinfo | 30022 | 30022 | 0 | $50B9 -> $50B9 |
| tasklist | 6022 | 6022 | 0 | $AE79 -> $AE79 |
| simplefiles | 12637 | 12637 | 0 | $94A2 -> $94A2 |
| game2048 | 28580 | 28580 | 0 | $565B -> $565B |
| deminer | 22167 | 22167 | 0 | $6F68 -> $6F68 |
| cal26 | 8704 | 8704 | 0 | $A3FF -> $A3FF |
| dizzy | 1834 | 1834 | 0 | $BED5 -> $BED5 |
| readyirc | 28959 | 28959 | 0 | $54E0 -> $54E0 |
| rirc-rrnet | 18173 | 18173 | 0 | $7F02 -> $7F02 |
| readybasic | 1031 | 1031 | 0 | $C1F8 -> $C1F8 |
| readme | 22748 | 22748 | 0 | $6D23 -> $6D23 |
| readyshell | 16585 | 16585 | 0 | $8536 -> $8536 |

## Dynamic Allocation Delta

Compared the current dynamic allocation build against committed bank `0` mirror
baseline `e7b5487`.

| App | Bank 0 baseline headroom | Dynamic headroom | Change | Runtime end |
|---|---:|---:|---:|---|
| cal26 | 8704 | 8703 | -1 | $A400 |
| calcplus | 6593 | 6593 | 0 | $AC3E |
| clipmgr | 13439 | 13439 | 0 | $9180 |
| deminer | 22167 | 22167 | 0 | $6F68 |
| dizzy | 1834 | 1834 | 0 | $BED5 |
| editor | 11665 | 11664 | -1 | $986F |
| game2048 | 28580 | 28580 | 0 | $565B |
| hexview | 32277 | 32277 | 0 | $47EA |
| launcher | 11122 | 8062 | -3060 | $A681 |
| quicknotes | 9471 | 9470 | -1 | $A101 |
| readme | 22748 | 22748 | 0 | $6D23 |
| readybasic | 1031 | 1031 | 0 | $C1F8 |
| readyirc | 28959 | 28958 | -1 | $54E1 |
| readyshell | 16585 | 16585 | 0 | $8536 |
| reuviewer | 29372 | 29372 | 0 | $5343 |
| rirc-rrnet | 18173 | 18172 | -1 | $7F03 |
| simplefiles | 12637 | 12636 | -1 | $94A3 |
| sysinfo | 30022 | 30021 | -1 | $50BA |
| tasklist | 6022 | 6021 | -1 | $AE7A |

Notes:

- The first dynamic implementation lost `8583` bytes in launcher headroom
  because a 64-entry catalog was duplicated in a resident resume cache. That
  cache was removed before acceptance.
- The accepted launcher delta is `-3060` bytes. This is primarily the real
  cost of growing resident catalog arrays from 24 to 65 entries plus dynamic
  allocation/unload code.
- The `-1` byte normal-app deltas come from broadening the shared hotkey helper
  to accept logical banks above `23`; no normal app gets the launcher allocator
  or bank `0` mirror module.

## ReadyShell `rsovl` Resource Delta

Compared the ReadyShell resource-token build against committed dynamic
allocation baseline `d366acb Clarify launcher-owned unload policy`.

| App | Dynamic baseline headroom | ReadyShell `rsovl` headroom | Change | Runtime end before -> after |
|---|---:|---:|---:|---|
| launcher | 8062 | 5075 | -2987 | $A681 -> $B22C |
| editor | 11664 | 11702 | +38 | $986F -> $9849 |
| quicknotes | 9470 | 9508 | +38 | $A101 -> $A0DB |
| calcplus | 6593 | 6683 | +90 | $AC3E -> $ABE4 |
| hexview | 32277 | 32367 | +90 | $47EA -> $4790 |
| clipmgr | 13439 | 13529 | +90 | $9180 -> $9126 |
| reuviewer | 29372 | 29606 | +234 | $5343 -> $5259 |
| sysinfo | 30021 | 30021 | 0 | $50BA -> $50BA |
| tasklist | 6021 | 6059 | +38 | $AE7A -> $AE54 |
| simplefiles | 12636 | 12636 | 0 | $94A3 -> $94A3 |
| game2048 | 28580 | 28580 | 0 | $565B -> $565B |
| deminer | 22167 | 22167 | 0 | $6F68 -> $6F68 |
| cal26 | 8703 | 8741 | +38 | $A400 -> $A3DA |
| dizzy | 1834 | 1834 | 0 | $BED5 -> $BED5 |
| readyirc | 28958 | 28996 | +38 | $54E1 -> $54BB |
| rirc-rrnet | 18172 | 18210 | +38 | $7F03 -> $7EDD |
| readybasic | 1031 | 1031 | 0 | $C1F8 -> $C1F8 |
| readme | 22748 | 22748 | 0 | $6D23 -> $6D23 |
| readyshell | 16585 | 18660 | +2075 | $8536 -> $7D1B |

Notes:

- Launcher pays the intentional `-2987` byte resident cost for ReadyShell
  resource allocation, overlay PRG streaming, resource ownership state, unload
  cleanup, and EasyFlash resource-set import.
- ReadyShell gains `2075` bytes because the legacy disk-side overlay
  self-loader/cache-writer fallback was removed. ReadyShell now only reads
  assigned bank metadata and fetches overlays from loader-filled REU slots.
- The shim remains unchanged. ReadyShell overlay cache banks are no longer
  fixed `$40/$41/$42` reservations and are not listed as fixed control-bank
  resources.
- ReadyBASIC remains unchanged and is still the tightest app-window case at
  `1031` bytes of headroom.
- The positive normal-app deltas come from the current rebuilt maps and profile
  churn after this milestone, not from linking ReadyShell resource code into
  those apps.
