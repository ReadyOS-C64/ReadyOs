# zlib6502 source reference

`inflatemem_cc65_upstream.s` is an unmodified source snapshot of the ca65 port
in the official cc65 repository at commit
`e11fb5c39371046ebe25485f984f644c5a0d65d3` (2026-08-20). That port derives
from Piotr Fusik's zlib6502 DEFLATE inflater. The original zlib6502 repository
was inspected at commit `77bacdd3893d9718f7ca1e59691e77c3c4dcd1c3`.

Upstream project: <https://github.com/pfusik/zlib6502>

This snapshot is reference material and is not linked directly. Ultimate ZIP's
production adaptation must be plainly marked as modified, retain this license,
use no cc65-reserved zero-page storage beyond declared imports, add bounded
stream I/O and malformed-input rejection, and use the ReadyOS 32K dictionary
window rather than assuming the whole input and output fit in memory.
