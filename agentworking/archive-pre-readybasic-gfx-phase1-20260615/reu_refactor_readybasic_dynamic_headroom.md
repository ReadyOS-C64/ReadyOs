# ReadyBASIC Dynamic RB Core Headroom

Captured on 2026-06-03 after making ReadyBASIC core/code REU banks
launcher-assigned `rbcore` resources instead of fixed `$44/$45` banks.

Before snapshot:

- `agentworking/reu_refactor_headroom_before_readybasic_dynamic.json`

After snapshot:

- `agentworking/reu_refactor_headroom_after_readybasic_dynamic.json`

| App | Before | After | Delta |
| --- | ---: | ---: | ---: |
| launcher | 5075 | 4637 | -438 |
| editor | 11702 | 11712 | +10 |
| quicknotes | 9508 | 9518 | +10 |
| calcplus | 6683 | 6715 | +32 |
| hexview | 32367 | 32399 | +32 |
| clipmgr | 13529 | 13561 | +32 |
| reuviewer | 29606 | 29670 | +64 |
| sysinfo | 30021 | 30021 | 0 |
| tasklist | 6059 | 6069 | +10 |
| simplefiles | 12636 | 12636 | 0 |
| game2048 | 28580 | 28580 | 0 |
| deminer | 22167 | 22167 | 0 |
| cal26 | 8741 | 8751 | +10 |
| dizzy | 1834 | 1834 | 0 |
| readyirc | 28996 | 29006 | +10 |
| rirc-rrnet | 18210 | 18220 | +10 |
| readybasic | 1031 | 1029 | -2 |
| readme | 22748 | 22748 | 0 |
| readyshell | 18660 | 18660 | 0 |

Acceptance notes:

- ReadyBASIC app-window headroom changed by `-2` bytes.
- Launcher headroom changed by `-438` bytes for launcher-owned `rbcore`
  allocation, load, and unload bookkeeping.
- The shim remains unchanged at the `$C800-$C9FF` ABI boundary.
- Normal apps do not take a shared-library penalty from the ReadyBASIC resolver.
