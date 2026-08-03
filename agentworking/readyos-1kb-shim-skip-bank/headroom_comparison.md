# ReadyOS Bank Refactor Memory Comparison

Generated from linker maps before and after the schema-v5 ReadyOS-bank refactor.
Positive headroom/heap deltas mean more free C64 RAM; negative code/data/BSS deltas mean smaller linked segments.

| App/map | Code/RO/init before | after | delta | Data before | after | delta | BSS before | after | delta | Heap before | after | delta | Window headroom before | after | delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| launcher | 39301 | 39295 | -6 | 50 | 50 | 0 | 2509 | 2509 | 0 | 5132 | 4626 | -506 | 5244 | 4738 | -506 |
| editor | 24384 | 24384 | 0 | 134 | 134 | 0 | 9880 | 9880 | 0 | 12594 | 12082 | -512 | 12706 | 12194 | -512 |
| quicknotes | 31031 | 31031 | 0 | 222 | 222 | 0 | 6047 | 6047 | 0 | 9692 | 9180 | -512 | 9804 | 9292 | -512 |
| calcplus | 37363 | 37363 | 0 | 158 | 158 | 0 | 1321 | 1321 | 0 | 8150 | 7638 | -512 | 8262 | 7750 | -512 |
| hexview | 12793 | 12793 | 0 | 46 | 46 | 0 | 101 | 101 | 0 | 34052 | 33540 | -512 | 34164 | 33652 | -512 |
| clipmgr | 30847 | 30847 | 0 | 46 | 46 | 0 | 1062 | 1062 | 0 | 15036 | 14524 | -512 | 15149 | 14637 | -512 |
| reuviewer | 15109 | 15105 | -4 | 46 | 46 | 0 | 221 | 221 | 0 | 31616 | 31108 | -508 | 31728 | 31220 | -508 |
| sysinfo | 16184 | 16184 | 0 | 47 | 47 | 0 | 277 | 277 | 0 | 30484 | 29972 | -512 | 30596 | 30084 | -512 |
| tasklist | 31116 | 31116 | 0 | 151 | 151 | 0 | 8927 | 8927 | 0 | 6798 | 6286 | -512 | 6910 | 6398 | -512 |
| simplefiles | 29693 | 29693 | 0 | 62 | 62 | 0 | 4397 | 4397 | 0 | 12840 | 12328 | -512 | 12952 | 12440 | -512 |
| simplecells | 34712 | 34712 | 0 | 238 | 238 | 0 | 2142 | 2142 | 0 | 9900 | 9388 | -512 | 10012 | 9500 | -512 |
| game2048 | 16838 | 16838 | 0 | 46 | 46 | 0 | 1203 | 1203 | 0 | 28904 | 28392 | -512 | 29017 | 28505 | -512 |
| deminer | 20178 | 20178 | 0 | 46 | 46 | 0 | 4276 | 4276 | 0 | 22492 | 21980 | -512 | 22604 | 22092 | -512 |
| sidetris | 15457 | 15457 | 0 | 86 | 86 | 0 | 908 | 908 | 0 | 30540 | 30028 | -512 | 30653 | 30141 | -512 |
| cal26 | 32292 | 32292 | 0 | 324 | 324 | 0 | 4803 | 4803 | 0 | 9572 | 9060 | -512 | 9685 | 9173 | -512 |
| dizzy | 38553 | 38553 | 0 | 132 | 132 | 0 | 6327 | 6327 | 0 | 1980 | 1468 | -512 | 2092 | 1580 | -512 |
| readyirc | 21882 | 21882 | 0 | 47 | 47 | 0 | 5351 | 5351 | 0 | 19712 | 19200 | -512 | 19824 | 19312 | -512 |
| rirc-rrnet | 21278 | 21278 | 0 | 227 | 227 | 0 | 6909 | 6909 | 0 | 18578 | 18066 | -512 | 18690 | 18178 | -512 |
| readybasic | 9290 | 9290 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | — | — | — | 1537 | 1025 | -512 |
| ucitest | 22520 | 22520 | 0 | 71 | 71 | 0 | 2708 | 2708 | 0 | 21692 | 21180 | -512 | 21805 | 21293 | -512 |
| readme | 24105 | 24105 | 0 | 46 | 46 | 0 | 48 | 48 | 0 | 22792 | 22280 | -512 | 22905 | 22393 | -512 |
| readyshell | 28069 | 28069 | 0 | 70 | 70 | 0 | 426 | 426 | 0 | 3688 | 3688 | 0 | 18539 | 18027 | -512 |
| launcher_easyflash | 26933 | 26927 | -6 | 50 | 50 | 0 | 1978 | 1978 | 0 | 18030 | 17524 | -506 | 18143 | 17637 | -506 |
| readyshell_easyflash | 27967 | 27967 | 0 | 70 | 70 | 0 | 426 | 426 | 0 | 3790 | 3790 | 0 | 18641 | 18129 | -512 |

Before snapshot end: `0xC7FF`. After snapshot end: `0xC5FF`.
ReadyBASIC has no conventional cc65 BSS/heap: its custom assembler/linker budget is enforced separately by `verify_readybasic_plugin.py`.
ReadyShell heap is bounded by its unchanged overlay load address; the full snapshot ends at `$C5FF` before the 1 KB resident shim.
