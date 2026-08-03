# ReadyOS Bank Refactor Memory Comparison

Generated from linker maps before and after the schema-v5 ReadyOS-bank refactor.
Positive headroom/heap deltas mean more free C64 RAM; negative code/data/BSS deltas mean smaller linked segments.

| App/map | Code/RO/init before | after | delta | Data before | after | delta | BSS before | after | delta | Heap before | after | delta | Window headroom before | after | delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| launcher | 39707 | 39301 | -406 | 50 | 50 | 0 | 2421 | 2509 | 88 | 4302 | 5132 | 830 | 4414 | 5244 | 830 |
| editor | 24533 | 24384 | -149 | 134 | 134 | 0 | 9880 | 9880 | 0 | 11932 | 12594 | 662 | 12045 | 12706 | 661 |
| quicknotes | 31159 | 31031 | -128 | 222 | 222 | 0 | 6047 | 6047 | 0 | 9052 | 9692 | 640 | 9164 | 9804 | 640 |
| calcplus | 37847 | 37363 | -484 | 158 | 158 | 0 | 1321 | 1321 | 0 | 7154 | 8150 | 996 | 7266 | 8262 | 996 |
| hexview | 13495 | 12793 | -702 | 46 | 46 | 0 | 101 | 101 | 0 | 32838 | 34052 | 1214 | 32950 | 34164 | 1214 |
| clipmgr | 31372 | 30847 | -525 | 46 | 46 | 0 | 1062 | 1062 | 0 | 14000 | 15036 | 1036 | 14112 | 15149 | 1037 |
| reuviewer | 16863 | 15109 | -1754 | 46 | 46 | 0 | 229 | 221 | -8 | 29342 | 31616 | 2274 | 29454 | 31728 | 2274 |
| sysinfo | 16247 | 16184 | -63 | 47 | 47 | 0 | 277 | 277 | 0 | 29908 | 30484 | 576 | 30021 | 30596 | 575 |
| tasklist | 31112 | 31116 | 4 | 151 | 151 | 0 | 8927 | 8927 | 0 | 6290 | 6798 | 508 | 6402 | 6910 | 508 |
| simplefiles | 29497 | 29693 | 196 | 62 | 62 | 0 | 4397 | 4397 | 0 | 12524 | 12840 | 316 | 12636 | 12952 | 316 |
| simplecells | 34805 | 34712 | -93 | 238 | 238 | 0 | 2142 | 2142 | 0 | 9294 | 9900 | 606 | 9407 | 10012 | 605 |
| game2048 | 16763 | 16838 | 75 | 46 | 46 | 0 | 1203 | 1203 | 0 | 28468 | 28904 | 436 | 28580 | 29017 | 437 |
| deminer | 20103 | 20178 | 75 | 46 | 46 | 0 | 4276 | 4276 | 0 | 22054 | 22492 | 438 | 22167 | 22604 | 437 |
| sidetris | 15312 | 15457 | 145 | 86 | 86 | 0 | 908 | 908 | 0 | 30174 | 30540 | 366 | 30286 | 30653 | 367 |
| cal26 | 32381 | 32292 | -89 | 324 | 324 | 0 | 4803 | 4803 | 0 | 8972 | 9572 | 600 | 9084 | 9685 | 601 |
| dizzy | 38299 | 38553 | 254 | 132 | 132 | 0 | 6327 | 6327 | 0 | 1722 | 1980 | 258 | 1834 | 2092 | 258 |
| readyirc | 22189 | 21882 | -307 | 47 | 47 | 0 | 5351 | 5351 | 0 | 18892 | 19712 | 820 | 19005 | 19824 | 819 |
| rirc-rrnet | 21585 | 21278 | -307 | 227 | 227 | 0 | 6909 | 6909 | 0 | 17758 | 18578 | 820 | 17871 | 18690 | 819 |
| readybasic | 9390 | 9290 | -100 | 0 | 0 | 0 | 0 | 0 | 0 | — | — | — | 1025 | 1537 | 512 |
| ucitest | 22583 | 22520 | -63 | 71 | 71 | 0 | 2708 | 2708 | 0 | 21118 | 21692 | 574 | 21230 | 21805 | 575 |
| readme | 24030 | 24105 | 75 | 46 | 46 | 0 | 48 | 48 | 0 | 22356 | 22792 | 436 | 22468 | 22905 | 437 |
| readyshell | 27910 | 28069 | 159 | 71 | 70 | -1 | 426 | 426 | 0 | 3846 | 3688 | -158 | 18185 | 18539 | 354 |
| launcher_easyflash | 27017 | 27043 | 26 | 50 | 50 | 0 | 1890 | 1978 | 88 | 17522 | 17920 | 398 | 17635 | 18033 | 398 |
| readyshell_easyflash | 27808 | 28149 | 341 | 71 | 71 | 0 | 426 | 426 | 0 | 3948 | 3608 | -340 | 18287 | 18458 | 171 |

Before snapshot end: `0xC5FF`. After snapshot end: `0xC7FF`.
ReadyBASIC has no conventional cc65 BSS/heap: its custom assembler/linker budget is enforced separately by `verify_readybasic_plugin.py`.
ReadyShell heap is bounded by its unchanged overlay load address, while its full snapshot headroom includes the new app-private `$C600-$C7FF` tail.
