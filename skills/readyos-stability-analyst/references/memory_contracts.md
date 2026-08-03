# ReadyOS Memory Contracts

Canonical sources:
- `MEMORY_MAP.md`
- `build_support/memory_map_spec.json`

## Critical RAM windows
- App runtime snapshot: `$1000-$C5FF` (`$B600` bytes)
- Shim resident region: `$C600-$C9FF` (1 KB)
- Shim expansion reserve: `$C600-$C7FF`; scalable state is authoritative in REU
- Public shim ABI: `$C800-$C9FF`
- Hardware I/O: `$D000-$DFFF`

## Shim jump/data anchors
- Jump table starts at `$C800`
- Shim data window: `$C820-$C83F`
- Current/target token and reserved legacy bytes: `$C834-$C83A`
- Direct physical ReadyOS-bank byte: `$C83B`

## REU physical/logical bank contract
- `Start` means physical REU bank `READYOS_REU_BANK_SKIP`.
- `Start+0` (`Skip`) is the combined ReadyOS bank: launcher snapshot
  `$0000-$B5FF` plus schema-v5 state `$B600-$FFFF`.
- `Start+1` (`Skip+1`) is the first dynamic allocation candidate; no launcher
  overlay or fixed app slot owns it.
- Shim token `0` resolves directly to the ReadyOS bank. Tokens `N > 0` resolve
  through the ReadyOS bank's `$B740-$B83F` lookup table.
- Loaded/resumable status is authoritative at ReadyOS `$B840-$B93F`; the
  retired `$C836-$C838` bitmap is reserved and must not drive navigation.
- The dynamic allocator begins at `Start+1` and skips banks already marked in
  use in the ReadyOS `$B640-$B73F` bank-type table.
- Physical REU size is launcher-owned. Banks beyond the detected physical end
  are marked `REU_UNAVAIL` in ReadyOS `$B640-$B73F` and reported by REU Viewer.

## ReadyShell overlay/resource contract
- `__HIMEM__ = $C600`
- Overlay size is profile-based:
- release/default (`READYSHELL_PARSE_TRACE_DEBUG=0`): `READYSHELL_OVERLAYSIZE = $3800`, `__OVERLAYSTART__ = $8E00`
- debug trace (`READYSHELL_PARSE_TRACE_DEBUG=1`): `READYSHELL_OVERLAYSIZE = $3B00`, `__OVERLAYSTART__ = $8B00`
- ReadyShell overlay cache banks are loader-assigned resources, not fixed
  physical `0x40/0x41` banks.
- ReadyShell's state/scratch/CAT/value/diagnostic arena is one loader-assigned
  resource bank, not fixed physical `0x43` or `0x48`.
- ReadyShell consumes a tiny generated runtime metadata block containing
  overlay `(bank, offset)` pairs. Use that metadata or logical REU bank `0`
  rich resource records when interpreting dumps.
- Current state-bank-relative layout:
- `$0000-$7DDF`: shared transient command scratch; CAT uses the front while active
- `$7DE0-$7FFF`: diagnostics/probe tail
- `$8000-$80FF`: heap metadata / command registry block
- `$80F0-$8113`: shared ReadyShell overlay metadata
- `$8120-$FEFF`: persistent REU value arena

## App-owned runtime allocation contract
- Apps that need runtime `REU_APP_ALLOC` banks may opt into
  `src/lib/reu_owned_alloc.c`.
- The primitive allocator remains `src/lib/reu_mgr_alloc.c`; apps that do not
  need ownership records do not pay for the owner-record writer.
- Owner-recorded runtime banks create `REUCB_DEP_KIND_APP_ALLOC` records in
  the ReadyOS rich-resource table at `$C240`, carrying owner app id, slot id, physical
  bank, and a four-character tag.
- Launcher unload frees owner-recorded `REU_APP_ALLOC` banks for the selected
  app; the shim does not participate.

Profile control commands:
- build release/default: `make -j1 READYSHELL_PARSE_TRACE_DEBUG=0`
- build debug trace: `make -j1 READYSHELL_PARSE_TRACE_DEBUG=1`
- verify release/default contract: `READYSHELL_PARSE_TRACE_DEBUG=0 python3 build_support/verify_memory_map.py`
- verify debug contract: `READYSHELL_PARSE_TRACE_DEBUG=1 python3 build_support/verify_memory_map.py`

## Hard gates
- `python3 build_support/verify_memory_map.py`
- `python3 build_support/verify_resume_contract.py`
- `python3 verify.py`

Contract drift in shim/app/REU reserved windows is a blocking failure.
