# ReadyOS 0.2.5 Shim and ReadyOS-Bank Architecture

This is the current implementation-backed explanation of the resident shim
and the combined ReadyOS bank. It was audited
against the `0.2.5` development tree on 2026-08-01.

The most important correction to older descriptions is that **physical
`Skip` is the ReadyOS bank and is the single source of truth**. It combines
the launcher snapshot and schema-v5 operating-system state:

```text
C64 RAM
$1000                                                         $C5FF $C600             $C9FF
|<--------------- active app snapshot: $B600 bytes -------------->|<-- 1 KB shim ------>|

ReadyOS physical REU region
Skip (ReadyOS bank)                       Skip+1 ... detected end
| launcher $0000-$B5FF                    | dynamic app/resource pool |
| schema v5 $B600-$FFFF                   | explicit token mappings   |
```

The configured skip is compiled into the build; resident shim byte `$C83B`
stores the direct physical ReadyOS bank number (`Skip`).

## What Changed

The older shim calculated every app snapshot's physical bank from a fixed
layout. A nonzero logical app bank `N` was tied to a reserved physical slot
after the global, launcher, and launcher-overlay reservations. That made the
resident shim depend on a fixed block of app slots and prevented the allocator
from using the REU as one flexible pool.

The current implementation makes these changes:

1. Physical `Skip` is the ReadyOS bank. It is skipped by the dynamic allocator,
   but it is not unused.
2. Physical `Skip+1` is the first dynamic app/resource bank.
3. Token `0` resolves directly to the ReadyOS bank;
   its first `$B600` bytes are the launcher snapshot.
4. Schema-v5 state occupies the same ReadyOS bank at `$B600-$FFFF`, including
   bank types, token mappings/status, clipboard metadata, hotkeys, settings,
   app/resource records, catalog text, audit data, and launcher runtime state.
5. For a nonzero token, the shim fetches one byte from
   `ReadyOS:$B740 + token`. That byte is the physical snapshot bank to use.
6. The old `$C600-$C7FF` allocation/clipboard/hotkey mirrors and the three-byte
   loaded bitmap are retired; `$C600-$C7FF` remains resident as contiguous shim
   expansion capacity rather than becoming app RAM.
7. The snapshot is `$1000-$C5FF` (`$B600` bytes), leaving a `$4A00`
   per-app resume tail at REU offsets `$B600-$FFFF`.
8. The full shim region is restored to 1 KB at `$C600-$C9FF`; the stable
   512-byte ABI and entry addresses remain at `$C800-$C9FF`. `$C81B` is the
   `mark_loaded` entry; `$C818` is a safe deprecated `RTS` target.

Mappings are explicitly allocated and published. No app or shim path may infer
a physical bank from a logical token arithmetically.

### Version-history clarification

The earlier 0.2.5 design used a separate control bank at `Skip` and launcher
bank at `Skip+1`. Schema v5 deliberately supersedes that split while preserving
the established transition entry points in the upper 512-byte ABI.
Ultimate DOS DMA remains launcher-side cold-load transport and publishes the
same explicit mappings as the KERNAL path.

## The State Was Split, Not Simply Moved

Only values required while `$1000-$C5FF` is being overwritten remain resident.
All scalable state is authoritative in the ReadyOS bank.

| State | Current location | Operational role / authority |
|---|---|---|
| transition target, current token, last-saved hint, bank skip, drive, flags | shim `$C820-$C83F` | resident transition state; available while app RAM is replaced |
| `$C836-$C838`, `$C83A` | resident reserved bytes | retired bitmap/log-index storage; never authoritative |
| active physical bank allocation types | ReadyOS `$B640-$B73F` | allocator authority used through direct byte/block DMA helpers |
| token-to-physical mapping | ReadyOS `$B740-$B83F` | live source read by shim, launcher, apps, and tools |
| token validity/loaded/resumable state | ReadyOS `$B840-$B93F` | authoritative transition status; committed only after a successful stash |
| clipboard metadata | ReadyOS `$B940-$B9CF` | authoritative 16-item table; payload banks remain dynamically allocated |
| global hotkeys | ReadyOS `$B9D0-$B9D8` | shared state read directly by focused TUI micromodules |
| launcher catalog-shape settings | ReadyOS `$B9D9-$B9FF` | `LS` v1 record used to reconstruct launcher arrays from the authoritative registry |
| app/resource/dependency/catalog records | ReadyOS `$BA00-$FB3F` | normalized 64-app registry and cold metadata |
| launcher executable snapshot | ReadyOS `$0000-$B5FF` | restored for token `0` |
| launcher runtime/resume record | ReadyOS `$FC40-$FCBF` | `RSM1`-validated UI state (selection, scroll, one-shot flag, and DMA-use flag/path when enabled) |

The ReadyOS bank is the source of truth. Resident shim bytes carry only the
minimum transition operands and one-byte DMA scratch needed while app RAM is
being replaced.

## Resident Shim (`$C600-$C9FF`) and ABI (`$C800-$C9FF`)

The resident shim owns exactly 1 KB outside the app snapshot window. Its lower
512 bytes are clear contiguous expansion reserve; its public ABI remains in the
upper 512 bytes at the same addresses as before.

### Measured shim room

The current image has **512 contiguous reserved bytes** at `$C600-$C7FF`, plus
**129 bytes of executable padding** within the established ABI, all verified as zero by
`verify_readyos_shim.py`. That space is fragmented: the largest single run is
42 bytes at `$C8B6-$C8DF`; the other runs are 33, 23, 7, 7, 5, 4, 4, 2, and 2
bytes. There are another 11 resident ABI/data bytes currently marked
scratch/retired/reserved (`$C81E-$C81F`, `$C822-$C823`, `$C832-$C833`,
`$C836-$C838`, `$C83A`, and `$C83F`), but they should not be counted as general
code room because existing callers may rely on their addresses or zero value.
So the practical extension budget is 512 contiguous bytes plus 129 fragmented
ABI padding bytes. Moving duplicate loaded/map/catalog authority into the
ReadyOS bank is what makes this budget possible; ABI padding still requires
extra caution because callers may depend on established addresses.

The 25-byte increase came from retiring five preload trace writes to
`$C007-$C00C`. No runtime or test consumed them, and app snapshot RAM is no
longer used as a shim diagnostic side channel. The reserved `$C812` entry now
jumps to the safe `$C9FF` `RTS` instead of into the middle of preload code.

### Stable entry block

| Address | Entry |
|---:|---|
| `$C800` | load from disk and run |
| `$C803` | fetch from REU and run |
| `$C806` | run app at `$1000` |
| `$C809` | preload app to REU and return |
| `$C80C` | return to launcher |
| `$C80F` | switch directly to another app |
| `$C812` | reserved compatibility slot; safe no-op jump to `$C9FF` |
| `$C815` | fetch-bank helper |
| `$C818` | deprecated logger entry; jumps to safe `RTS` at `$C9FF` |
| `$C81B` | mark token loaded/resumable after a successful stash |

### Resident data bytes

| Address | Field | Current meaning |
|---:|---|---|
| `$C820` | `target_bank` | target logical snapshot token |
| `$C821` | `filename_len` | filename length for KERNAL load paths |
| `$C822-$C823` | app-size scratch ABI bytes | launcher uses them; shim does not consume them |
| `$C824-$C82F` | `filename[12]` | disk-load filename buffer |
| `$C830-$C831` | `load_end` | actual end returned by KERNAL `LOAD` during preload |
| `$C832-$C833` | unused | reserved |
| `$C834` | `current_bank` | currently running logical snapshot token |
| `$C835` | `last_saved` | recovery/resilience hint |
| `$C836-$C838` | reserved | retired loaded bitmap bytes |
| `$C839` | `storage_drive` | shared default file-dialog drive |
| `$C83A` | reserved | retired debug-ring index |
| `$C83B` | `readyos_bank` | direct physical ReadyOS bank (`Skip`) |
| `$C83C` | `launcher_flags` | launcher-owned one-shot flags |
| `$C83D` | `reu_lookup_scratch` | physical/status byte DMA scratch |
| `$C83E` | `token_scratch` | token preserved by `mark_loaded` |
| `$C83F` | reserved | future ABI space |

### Lookup flow at `$C960`

For shim token `0`:

1. read `$C83B`
2. use that direct physical ReadyOS-bank number

For any nonzero token `N`:

1. configure a one-byte REU fetch into `$C83D`
2. fetch from the ReadyOS bank at offset `$B740 + N`
3. load the fetched physical bank byte
4. configure the normal `$B600` stash or fetch at C64 address `$1000`

The small one-byte preliminary DMA is why the shim can resolve a scalable
logical token without carrying a large resident table.

## ReadyOS Bank (`Skip`, Schema 5)

| Offset | Size | Contents |
|---:|---:|---|
| `$0000` | `$B600` | launcher snapshot of C64 `$1000-$C5FF` |
| `$B600` | `$0040` | `RCB5` header: schema, writer, skip/count/availability and offsets |
| `$B640` | `$0100` | authoritative 256-entry physical bank-type table |
| `$B740` | `$0100` | token-to-physical snapshot map |
| `$B840` | `$0100` | token valid/loaded/resumable status |
| `$B940` | `$0090` | clipboard count and 16 item records |
| `$B9D0` | `9` | global hotkeys |
| `$B9D9` | `$0027` | active `LS` v1 launcher settings: first-app index, app count, load-all policy, and 32-byte variant name |
| `$BA00` | `$0400` | 64 app records (`64 x 16`) |
| `$BE00` | `$0100` | token-to-app-index table |
| `$BF00` | `$0340` | 64 app filename records (`64 x 13`) |
| `$C240` | `$0400` | 64 rich resource records (`64 x 16`) |
| `$C640` | `$2000` | 64 dependency/source lines (`64 x 128`) |
| `$E640` | `$0800` | catalog names (`64 x 32`) |
| `$EE40` | `$09C0` | catalog descriptions (`64 x 39`) |
| `$F800` | `$0340` | catalog file tokens (`64 x 13`) |
| `$FB40` | `$0100` | audit page |
| `$FC40` | `$0080` | launcher `RSM1` envelope: 16-byte header plus compact UI payload; DMA builds also persist the bounded image path |
| `$FCC0` | `$0340` | reserved for compatible schema growth |

`reu_control_bank_sync_and_mirror()` writes bank types directly to the ReadyOS
bank; there is no resident mirror to reconcile. The launcher registry writer
publishes the catalog, explicit token maps/status, physical snapshot sizes, and
resource ownership. Banks beyond the detected physical size are marked
`REU_UNAVAIL` and are never eligible for allocation.

The launcher deliberately does not persist its former full set of parallel
arrays in the resume tail. On return, it validates the small `RSM1` UI record,
then rebuilds `app_banks`, drives, hotkeys, resource-bank assignments,
loaded flags, and snapshot sizes from the `LS` settings record plus the 64 app
records at `$BA00`. Selection bounds are checked only after that registry has
restored the menu shape. Catalog strings remain in their ReadyOS-bank tables
and are fetched through a small visible-row cache and one shared scratch
buffer.

Because that scratch buffer is also used for ReadyOS DMA publication, code may
not retain a pointer returned by a catalog accessor across a metadata write.
The preload path resolves/allocates its token first and only then fetches the
filename; the DMA fallback reacquires the filename after republishing state.

## App Switching With the Split State

### Preload

1. Launcher assigns a logical snapshot token and physical slot.
2. Launcher publishes the allocation, map, and status directly to ReadyOS.
3. Shim stashes the launcher using token `0` to the ReadyOS bank.
4. The app is loaded into `$1000`, then stashed using its nonzero token.
5. Shim reads that token's physical bank from ReadyOS `$B740` and performs the transfer.
6. Shim restores the launcher from the ReadyOS bank.

### Return or direct switch

1. Shim uses resident `$C834`/`$C820` to know the current and target tokens.
2. Each nonzero token is resolved through ReadyOS `$B740`.
3. Shim stashes/fetches exactly `$B600` bytes.
4. Returning to the launcher resolves token `0` directly to `$C83B`.

The shim never needs the launcher's normal app RAM structures to survive this
sequence; that is the reason the small resident state remains resident.

## Relationship to 0.2.5 Ultimate DOS DMA Loading

The opt-in Ultimate DOS/UCI DMA path changes how a cold PRG payload reaches an
already allocated REU snapshot bank. It does **not** enlarge or relocate the
resident shim and does not change the later switch contract:

- the destination snapshot is still launcher-allocated;
- ReadyOS-bank metadata, mapping, and status are still republished;
- later app launches, returns, and direct switches still use the resident shim
  and its `$B600` transfer;
- the KERNAL/disk preload path remains the fallback.

The current non-DMA and DMA launchers therefore share the same resident
shim/ReadyOS-bank switching architecture.

## Source of Truth

- resident byte image: `src/boot/readyos_shim.inc`
- ReadyOS-bank schema and publisher (retaining the source module's historical
  filename): `src/lib/reu_control_bank.h` and
  `src/lib/reu_control_bank.c`
- normalized app registry publisher: `src/lib/reu_control_registry.c`
- active allocator and mapping helpers: `src/lib/reu_mgr*.c`,
  `src/lib/reu_mgr.h`, and `src/apps/launcher/launcher.c`
- canonical detailed map: `privatedocs/top_level_md/MEMORY_MAP.md`

Older shim HTML reports are retained as historical architecture evidence. They
must not be read as current when they show a separate control bank, a resident
allocation/bitmap mirror, fixed app slots, `$2F00` lookup, or `$B800` snapshots.

## Full Commented Resident Shim Source

This appendix is synchronized from `src/boot/readyos_shim.inc`; both the disk
and EasyFlash builds use that same canonical 1024-byte definition. The disk
bootstrap clears the lower reserve in place and embeds only the upper ABI half
to stay below `$1000`; EasyFlash packages the complete image. The documentation
verifier compares every directive and ABI annotation.

```asm
;-----------------------------------------------------------------------------
; Shared ReadyOS shim image ($C600-$C9FF, 1024 bytes)
; This file is the canonical shim byte layout used by both the disk boot path
; and the EasyFlash cartridge flavor. Keep it identical across all variants.
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; Pages 1-2: $C600-$C7FF - reserved resident expansion space
;-----------------------------------------------------------------------------
; This space used to contain the C64-RAM REU allocation/metadata mirror.  The
; authoritative state now lives in the ReadyOS REU bank, but the RAM remains
; resident shim-owned capacity rather than becoming app snapshot memory.
.ifndef READYOS_SHIM_ABI_ONLY
.res $0200, $00
.endif

;-----------------------------------------------------------------------------
; Page 3: $C800-$C8FF - Jump table, data, and helper routines
;-----------------------------------------------------------------------------

; $C800-$C817: Jump Table (24 bytes, 8 entries)
jt_load_disk    = $C800     ; Load from disk, run
jt_load_reu     = $C803     ; Fetch from REU, run
jt_run_app      = $C806     ; Just run app
jt_preload      = $C809     ; Preload to REU, return
jt_return       = $C80C     ; Return to launcher
jt_switch       = $C80F     ; Switch to another app
jt_reserved     = $C812     ; Reserved compatibility slot (safe no-op)
jt_fetch_bank   = $C815     ; Helper: fetch from bank in A

.byte $4C, $40, $C8         ; $C800: JMP load_disk ($C840)
.byte $4C, $60, $C8         ; $C803: JMP load_reu ($C860)
.byte $4C, $00, $10         ; $C806: JMP $1000 (run app)
.byte $4C, $80, $C8         ; $C809: JMP preload ($C880)
.byte $4C, $00, $C9         ; $C80C: JMP return_to_launcher ($C900)
.byte $4C, $40, $C9         ; $C80F: JMP switch_app ($C940)
.byte $4C, $FF, $C9         ; $C812: reserved helper -> safe RTS
.byte $4C, $F0, $C8         ; $C815: JMP fetch_bank ($C8F0)

; $C818: deprecated logger compatibility slot
.byte $4C, $FF, $C9         ; $C818: deprecated logger -> safe RTS
.byte $4C, $C0, $C9         ; $C81B: JMP mark_loaded (token in A)
.byte $00,$00               ; $C81E-$C81F: reserved

; $C820-$C83F: Data Area (32 bytes)
; $C820: target_bank - bank to load/switch to
; $C821: filename_len
; $C822-$C823: launcher-side app-size scratch (not consumed by the shim)
; $C824-$C82F: filename (12 bytes)
; $C830: load_end_lo (KERNAL LOAD end addr low byte, saved by preload)
; $C831: load_end_hi (KERNAL LOAD end addr high byte, saved by preload)
; $C832-$C833: unused
; $C834: current_bank - currently running app
; $C835: last_saved
; $C836-$C838: retired bitmap bytes (reserved; authoritative state is in REU)
; $C839: storage_drive - shim-global default storage drive for D8/D9 app dialogs
;        This persists across app switches and is shared by apps that use the
;        common file-dialog default-drive contract.
; $C83A: reserved (formerly log_index)
; $C83B: readyos_bank - direct physical ReadyOS bank number (Skip)
; $C83C: launcher_flags - launcher-owned one-shot state flags
; $C83D: reu_lookup_scratch - one-byte ReadyOS-bank DMA scratch
; $C83E: token_scratch; $C83F: reserved

.byte $00                   ; $C820: target_bank
.byte $08                   ; $C821: filename_len
.byte $00, $00              ; $C822-$C823: launcher app-size scratch
.byte "LAUNCHER    "        ; $C824-$C82F: filename (12 bytes)
.byte $00,$00               ; $C830-$C831: load_end_lo, load_end_hi
.byte $00,$00               ; $C832-$C833: unused
.byte $00                   ; $C834: current_bank
.byte $FF                   ; $C835: last_saved
.byte $00                   ; $C836: reserved (retired bitmap byte)
.byte $00                   ; $C837: reserved (retired bitmap byte)
.byte $00                   ; $C838: reserved (retired bitmap byte)
.byte $08                   ; $C839: storage_drive (default drive 8)
.byte $00                   ; $C83A: reserved (retired debug-ring head)
.byte READYOS_REU_BANK_SKIP     ; $C83B: readyos_bank (physical Skip)
.byte $00                   ; $C83C: launcher_flags
.byte $00                   ; $C83D: reu_lookup_scratch
.byte $00,$00               ; $C83E-$C83F: reserved

;-----------------------------------------------------------------------------
; $C840: load_disk - Load app from disk and run (32 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $21, $C8         ; LDA $C821 (filename len)
.byte $A2, $24              ; LDX #$24
.byte $A0, $C8              ; LDY #$C8 (filename at $C824)
.byte $20, $BD, $FF         ; JSR $FFBD (SETNAM)
.byte $A9, $00              ; LDA #0
.byte $AE, $39, $C8         ; LDX $C839 (storage drive)
.byte $A0, $01              ; LDY #1
.byte $20, $BA, $FF         ; JSR $FFBA (SETLFS)
.byte $A9, $00              ; LDA #0
.byte $20, $D5, $FF         ; JSR $FFD5 (LOAD)
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00       ; Padding to $C860

;-----------------------------------------------------------------------------
; $C860: load_reu - Fetch app from REU and run (32 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C880: preload - Load to REU, return to launcher
; Called via JSR $C809
;-----------------------------------------------------------------------------
.byte $A9, $00              ; LDA #0 (launcher token -> ReadyOS bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)

.byte $AD, $21, $C8         ; LDA $C821 (len)
.byte $A2, $24              ; LDX #$24
.byte $A0, $C8              ; LDY #$C8 (filename at $C824)
.byte $20, $BD, $FF         ; JSR SETNAM

.byte $A9, $00              ; LDA #0
.byte $AE, $39, $C8         ; LDX $C839 (storage drive)
.byte $A0, $01              ; LDY #1
.byte $20, $BA, $FF         ; JSR SETLFS

.byte $A9, $00              ; LDA #0
.byte $20, $D5, $FF         ; JSR LOAD

.byte $8E, $30, $C8         ; STX $C830 (end addr lo)
.byte $8C, $31, $C8         ; STY $C831 (end addr hi)

.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)

.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $C0, $C9         ; JSR mark_loaded ($C9C0)

.byte $A9, $00              ; LDA #0
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)

.byte $60                   ; RTS

.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00

;-----------------------------------------------------------------------------
; $C8E0: stash_to_bank - Stash $1000-$C5FF to REU bank in A (16 bytes)
;-----------------------------------------------------------------------------
.byte $20, $60, $C9         ; JSR reu_setup_logical ($C960)
.byte $A9, $90              ; LDA #$90 (STASH command)
.byte $8D, $01, $DF         ; STA $DF01 - execute transfer
.byte $60                   ; RTS
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C8F0: fetch_bank - Fetch from REU bank in A to $1000-$C5FF (16 bytes)
;-----------------------------------------------------------------------------
.byte $20, $60, $C9         ; JSR reu_setup_logical ($C960)
.byte $A9, $91              ; LDA #$91 (FETCH command)
.byte $8D, $01, $DF         ; STA $DF01 - execute transfer
.byte $60                   ; RTS
.byte $00,$00,$00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; Page 4: $C900-$C9FF - Main routines
;-----------------------------------------------------------------------------

;-----------------------------------------------------------------------------
; $C900: return_to_launcher (64 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $C0, $C9         ; JSR mark_loaded ($C9C0)
.byte $AD, $34, $C8         ; LDA $C834
.byte $8D, $35, $C8         ; STA $C835
.byte $A9, $00              ; LDA #0
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)
.byte $A9, $00              ; LDA #0
.byte $8D, $34, $C8         ; STA $C834
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00,$00,$00,$00,$00,$00,$00,$00
.byte $00

;-----------------------------------------------------------------------------
; $C940: switch_app (64 bytes)
;-----------------------------------------------------------------------------
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $E0, $C8         ; JSR stash_to_bank ($C8E0)
.byte $AD, $34, $C8         ; LDA $C834 (current bank)
.byte $20, $C0, $C9         ; JSR mark_loaded ($C9C0)
.byte $AD, $20, $C8         ; LDA $C820 (target bank)
.byte $20, $F0, $C8         ; JSR fetch_bank ($C8F0)
.byte $AD, $20, $C8         ; LDA $C820
.byte $8D, $34, $C8         ; STA $C834
.byte $4C, $00, $10         ; JMP $1000
.byte $00,$00,$00,$00,$00

;-----------------------------------------------------------------------------
; $C960: reu_setup_logical - Resolve token via the ReadyOS bank, then setup regs
;-----------------------------------------------------------------------------
.byte $C9, $00              ; CMP #0 (logical launcher bank?)
.byte $D0, $06              ; BNE lookup_app_bank
.byte $AD, $3B, $C8         ; LDA $C83B (ReadyOS bank contains launcher)
.byte $4C, $A0, $C9         ; JMP reu_setup ($C9A0)
.byte $AA                   ; lookup_app_bank: TAX (preserve logical token)
.byte $A9, $3D              ; LDA #<$C83D
.byte $8D, $02, $DF         ; STA $DF02 (C64 addr lo)
.byte $A9, $C8              ; LDA #>$C83D
.byte $8D, $03, $DF         ; STA $DF03 (C64 addr hi)
.byte $8A                   ; TXA
.byte $18                   ; CLC
.byte $69, $40              ; ADC #<$B740 (mapping offset + token)
.byte $8D, $04, $DF         ; STA $DF04 (REU offset lo)
.byte $A9, $B7              ; LDA #>$B740
.byte $69, $00              ; ADC #0 (include low-byte carry)
.byte $8D, $05, $DF         ; STA $DF05 (REU offset hi)
.byte $AD, $3B, $C8         ; LDA $C83B (ReadyOS global physical bank)
.byte $8D, $06, $DF         ; STA $DF06
.byte $A9, $01              ; LDA #1
.byte $8D, $07, $DF         ; STA $DF07 (length lo)
.byte $A9, $00              ; LDA #0
.byte $8D, $08, $DF         ; STA $DF08 (length hi)
.byte $A9, $91              ; LDA #$91 (FETCH command)
.byte $8D, $01, $DF         ; STA $DF01 - fetch physical bank byte
.byte $AD, $3D, $C8         ; LDA $C83D (resolved physical bank)
.byte $4C, $A0, $C9         ; JMP reu_setup ($C9A0)
.byte $00,$00               ; Padding to $C9A0

;-----------------------------------------------------------------------------
; $C9A0: reu_setup - Set up REU registers for $B600 transfer at $1000
;-----------------------------------------------------------------------------
.byte $8D, $06, $DF         ; STA $DF06 (bank)
.byte $A9, $00              ; LDA #$00
.byte $8D, $02, $DF         ; STA $DF02 (C64 addr lo = $00)
.byte $A9, $10              ; LDA #$10
.byte $8D, $03, $DF         ; STA $DF03 (C64 addr hi = $10, so $1000)
.byte $A9, $00              ; LDA #$00
.byte $8D, $04, $DF         ; STA $DF04 (REU addr lo)
.byte $8D, $05, $DF         ; STA $DF05 (REU addr hi)
.byte $8D, $07, $DF         ; STA $DF07 (len lo = $00)
.byte $A9, $B6              ; LDA #$B6
.byte $8D, $08, $DF         ; STA $DF08 (len hi = $B6, so $B600 bytes)
.byte $60                   ; RTS
.byte $00,$00

;-----------------------------------------------------------------------------
; $C9C0: mark_loaded - Commit token status after a successful snapshot stash
; A = logical token. Status $07 means valid, loaded, and resumable.
;-----------------------------------------------------------------------------
.byte $8D, $3E, $C8         ; STA $C83E (preserve token)
.byte $A9, $07              ; LDA #VALID|LOADED|RESUMABLE
.byte $8D, $3D, $C8         ; STA $C83D
.byte $A9, $3D              ; C64 source = $C83D
.byte $8D, $02, $DF
.byte $A9, $C8
.byte $8D, $03, $DF
.byte $AD, $3E, $C8         ; token
.byte $18                   ; CLC
.byte $69, $40              ; + <$B840 (token status table)
.byte $8D, $04, $DF
.byte $A9, $B8              ; >$B840
.byte $69, $00              ; include low-byte carry
.byte $8D, $05, $DF
.byte $AD, $3B, $C8         ; ReadyOS physical bank
.byte $8D, $06, $DF
.byte $A9, $01              ; one byte
.byte $8D, $07, $DF
.byte $A9, $00
.byte $8D, $08, $DF
.byte $A9, $90              ; STASH C64 byte to ReadyOS bank
.byte $8D, $01, $DF
.byte $AD, $3E, $C8         ; preserve caller-visible token in A
.byte $60                   ; RTS
.byte $00,$00,$00,$00       ; reserved through $C9FE
.byte $60                   ; $C9FF: compatibility no-op RTS
```
