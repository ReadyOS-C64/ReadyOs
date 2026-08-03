# ReadyOS Bank Refactor Memory Comparison

Generated from linker maps before and after the schema-v5 ReadyOS-bank refactor.
Positive headroom/heap deltas mean more free C64 RAM; negative code/data/BSS deltas mean smaller linked segments.

| App/map | Code/RO/init before | after | delta | Data before | after | delta | BSS before | after | delta | Heap before | after | delta | Window headroom before | after | delta |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| launcher | 39707 | 39325 | -382 | 50 | 50 | 0 | 2421 | 2509 | 88 | 4302 | 5108 | 806 | 4414 | 5220 | 806 |
| editor | 24533 | 24408 | -125 | 134 | 134 | 0 | 9880 | 9880 | 0 | 11932 | 12570 | 638 | 12045 | 12682 | 637 |
| quicknotes | 31159 | 31055 | -104 | 222 | 222 | 0 | 6047 | 6047 | 0 | 9052 | 9668 | 616 | 9164 | 9780 | 616 |
| calcplus | 37847 | 37397 | -450 | 158 | 158 | 0 | 1321 | 1321 | 0 | 7154 | 8116 | 962 | 7266 | 8228 | 962 |
| hexview | 13495 | 12827 | -668 | 46 | 46 | 0 | 101 | 101 | 0 | 32838 | 34018 | 1180 | 32950 | 34130 | 1180 |
| clipmgr | 31372 | 30881 | -491 | 46 | 46 | 0 | 1062 | 1062 | 0 | 14000 | 15002 | 1002 | 14112 | 15115 | 1003 |
| reuviewer | 16863 | 15143 | -1720 | 46 | 46 | 0 | 229 | 221 | -8 | 29342 | 31582 | 2240 | 29454 | 31694 | 2240 |
| sysinfo | 16247 | 16184 | -63 | 47 | 47 | 0 | 277 | 277 | 0 | 29908 | 30484 | 576 | 30021 | 30596 | 575 |
| tasklist | 31112 | 31140 | 28 | 151 | 151 | 0 | 8927 | 8927 | 0 | 6290 | 6774 | 484 | 6402 | 6886 | 484 |
| simplefiles | 29497 | 29717 | 220 | 62 | 62 | 0 | 4397 | 4397 | 0 | 12524 | 12816 | 292 | 12636 | 12928 | 292 |
| simplecells | 34805 | 34736 | -69 | 238 | 238 | 0 | 2142 | 2142 | 0 | 9294 | 9876 | 582 | 9407 | 9988 | 581 |
| game2048 | 16763 | 16872 | 109 | 46 | 46 | 0 | 1203 | 1203 | 0 | 28468 | 28870 | 402 | 28580 | 28983 | 403 |
| deminer | 20103 | 20212 | 109 | 46 | 46 | 0 | 4276 | 4276 | 0 | 22054 | 22458 | 404 | 22167 | 22570 | 403 |
| sidetris | 15312 | 15481 | 169 | 86 | 86 | 0 | 908 | 908 | 0 | 30174 | 30516 | 342 | 30286 | 30629 | 343 |
| cal26 | 32381 | 32316 | -65 | 324 | 324 | 0 | 4803 | 4803 | 0 | 8972 | 9548 | 576 | 9084 | 9661 | 577 |
| dizzy | 38299 | 38587 | 288 | 132 | 132 | 0 | 6327 | 6327 | 0 | 1722 | 1946 | 224 | 1834 | 2058 | 224 |
| readyirc | 22189 | 21906 | -283 | 47 | 47 | 0 | 5351 | 5351 | 0 | 18892 | 19688 | 796 | 19005 | 19800 | 795 |
| rirc-rrnet | 21585 | 21302 | -283 | 227 | 227 | 0 | 6909 | 6909 | 0 | 17758 | 18554 | 796 | 17871 | 18666 | 795 |
| readybasic | 9390 | 9290 | -100 | 0 | 0 | 0 | 0 | 0 | 0 | — | — | — | 1025 | 1537 | 512 |
| ucitest | 22583 | 22520 | -63 | 71 | 71 | 0 | 2708 | 2708 | 0 | 21118 | 21692 | 574 | 21230 | 21805 | 575 |
| readme | 24030 | 24139 | 109 | 46 | 46 | 0 | 48 | 48 | 0 | 22356 | 22758 | 402 | 22468 | 22871 | 403 |
| readyshell | 27910 | 28188 | 278 | 71 | 71 | 0 | 426 | 426 | 0 | 3846 | 3568 | -278 | 18185 | 18419 | 234 |
| launcher_easyflash | 27017 | 27043 | 26 | 50 | 50 | 0 | 1890 | 1978 | 88 | 17522 | 17920 | 398 | 17635 | 18033 | 398 |
| readyshell_easyflash | 27808 | 28149 | 341 | 71 | 71 | 0 | 426 | 426 | 0 | 3948 | 3608 | -340 | 18287 | 18458 | 171 |

Before snapshot end: `0xC5FF`. After snapshot end: `0xC7FF`.
ReadyBASIC has no conventional cc65 BSS/heap: its custom assembler/linker budget is enforced separately by `verify_readybasic_plugin.py`.
ReadyShell heap is bounded by its unchanged overlay load address, while its full snapshot headroom includes the new app-private `$C600-$C7FF` tail.
