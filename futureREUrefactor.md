# Future REU Refactor Plan

> **Implemented by the schema-v5 ReadyOS-bank refactor (2026-08-01).** This
> plan is preserved rather than deleted. Current authority is physical
> `Skip+1`, the ReadyOS bank; `$C600-$C7FF` is app-private, physical `Skip` is
> the first dynamic bank, and token mapping/status live at `$B940/$BA40`.
> See `privatedocs/top_level_md/MEMORY_MAP.md` for the current contract.

## Purpose

This was the active future plan after Phase 1 of the ReadyOS REU refactor.
The completed Phase 1 record lives in `ReadyOSREUPhase1Completed.md`.

The future work here builds on a working v4 REU control-bank model. The goal is
not to make every app parse arbitrary config at runtime. The goal is to make
the launcher and REU bank `0` accurately represent what was requested,
allocated, loaded, selected, failed, and owned, while apps consume only tiny
contracts appropriate to their memory budgets.

## Current State

Phase 1 completed:

- logical REU bank `0` is the ReadyOS control bank using `RCB0` schema version
  `4`;
- the resident `$C600-$C9FF` shim-adjacent area remains the same `1KB` shape:
  - `$C600-$C6FF`: hot 256-byte bank allocation/type table;
  - `$C700-$C7FF`: existing resident metadata/reserved area;
  - `$C800-$C9FF`: 512-byte resident shim;
- the resident shim remains exactly `512` bytes;
- app-token lookup no longer depends on fixed reserved app slots;
- app snapshots are allocated lazily/dynamically;
- the launcher supports 64-entry catalogs while keeping large catalog text in
  REU bank `0`;
- disk `apps.cfg` and app manifests can carry dependency/resource lines for
  current concrete contracts;
- ReadyShell overlay cache banks, state/scratch, CAT staging, and diagnostic
  bytes are loader-owned dynamic resources;
- ReadyBASIC core/code resources are launcher/cartridge-assigned rather than
  fixed physical banks;
- app-requested runtime banks can be recorded as app-owned resources with a
  tiny optional micromodule instead of growing the primitive allocator;
- launcher-owned unload frees app snapshots, launcher-owned resources, and
  recorded app-owned runtime banks;
- REU Viewer reads bank `0` relationship metadata;
- physical REU size is launcher-owned and mirrored into bank `0`;
- regular ReadyBASIC, regular ReadyShell, and full EasyFlash/cartridge VICE
  suites passed after the physical-size checkpoint.

Current important headroom snapshot:

| App | Headroom |
| --- | ---: |
| launcher | `5242` bytes |
| launcher_easyflash | `17757` bytes |
| readybasic | `1029` bytes |
| readyshell | `18185` bytes |
| quicknotes | `9164` bytes |
| readyirc | `28657` bytes |
| rirc-rrnet | `17871` bytes |
| reuviewer | `29454` bytes |

ReadyBASIC remains the tightest memory contract. Future shared helpers must not
quietly make ReadyBASIC pay for launcher/registry features.

## Core Direction

The next resource model should make the **physical bank assignment** dynamic,
not force each app to understand arbitrary placement data.

For example, ReadyShell may still be compiled to expect:

- resource bank ordinal `0`: several overlays at known offsets;
- resource bank ordinal `1`: several overlays at known offsets;
- resource bank ordinal `2`: remaining overlay(s);
- resource bank ordinal `3`: state/scratch/CAT/diagnostic arena.

The launcher decides which physical banks back those ordinals. The app uses a
small assigned-bank contract and does not parse `apps.cfg`.

Future apps with optional plugins may need more than this. They may need to
ask: which plugin resource sets were selected, loaded, failed, or unloaded?
That should be supported by compact REU bank `0` records and optional tiny
micromodules, not by linking the launcher parser into apps.

## Current Bank 0 Sufficiency

The current v4 records are sufficient for Phase 1:

- REU Viewer display of current app/resource ownership;
- launcher-owned unload of snapshots, loader-owned resources, and recorded
  app-owned runtime allocations;
- ReadyShell and ReadyBASIC fixed-contract resource loading;
- simple app-requested resource ownership via `REUCB_DEP_KIND_APP_ALLOC`
  records containing owner app id, slot id, physical bank, and a short tag;
- debugging which physical banks were assigned;
- physical REU size and unavailable-tail reporting.

The current owner-recorded app allocation shape is intentionally small. It is
sufficient for banks like QuickNotes note storage and IRC scrollback, where the
app asks for one or a few banks and only needs ownership/unload visibility. It
is not a plugin/resource-set schema; future plugin apps still need the resource
set layer described below.

They are not yet sufficient for a future optional plugin manager because they
do not fully represent:

- first-class resource-set definitions;
- dynamic resource-set names;
- requested vs selected vs loaded vs failed state;
- optional vs required items;
- plugin/module class and role semantics beyond today's hard-coded kinds;
- explicit bank ordinal within a resource set independent of physical bank;
- enough capacity/paging strategy for many plugins across several apps.

## Future Resource Schema Goal

Add a small resource-set layer above the existing resource item records.

Resource set record:

```text
set_id
owner_app
resource_class
flags
bank_count
selected_count
loaded_count
name_ref_or_tag
layout_contract_id
first_item_record
```

Resource item record:

```text
set_id
item_id_or_slot
role_or_kind
bank_ordinal
physical_bank
offset
length
flags
source_ref_or_tag
next_item_record
```

Required flags should cover at least:

- available;
- selected/requested;
- loaded;
- failed;
- required;
- optional;
- unloadable;
- resident;
- app-private;
- service-temp, later.

Resource classes should remain small numeric codes. Candidate classes:

- `pack`: load files into assigned 64K banks at explicit offsets;
- `roles`: assign named role banks such as `core`, `code`, `dict`, `data`;
- `scratch`: assign app-owned scratch/state banks;
- `plugin-pack`: optional plugin/module set with selected/loaded state;
- `service-temp`: later temporary service invocation resources.

The resource-set name is metadata. Behavior comes from the numeric class and
layout contract, not from arbitrary string comparison in every app.

## Config Direction

Replace magic behavior strings like `rsovl` and `rbcore` with a compact
resource-set descriptor. The exact syntax can still be tuned, but the shape
should be positional and cheap for the C64 launcher to parse.

Example direction:

```text
8:readyshell:ready shell (demo)::pack:4:rshell-v4+
command shell poc scaffold
rsparser@0:0000,rsvm@0:3800,rsdrvilst@0:7000,rsldv@1:0000,rsstv@0:a800,rsfops@1:3800,rscat@1:7000,rscopy@1:a800,rsedit@2:0000
```

```text
9:readybasic:ready basic (alpha):3:roles:2:rbasic-v1+
scoped basic v2 bridge poc
core=rbcore@0:0000,code=rbcode@1:0000
```

For future optional plugin apps:

```text
9:editor:editor:1:plugin-pack:2:editor-tools-v1+
text editor with plugins
spell?=edspell@0:0000,fmt?=edfmt@0:3000,grep=edgrep@1:0000
```

The exact optional marker can change. The important contract is that the
launcher records both what existed in the manifest and what it actually loaded.

## App Runtime Contract

Apps should not parse the config file. Possible app-side contracts:

1. **Fixed app contract, launcher translated**
   - ReadyShell continues to receive its tiny overlay bank/offset metadata.
   - ReadyBASIC continues to receive its assigned core/code bank contract.
   - App impact: `0` or near `0`.

2. **Small resource lookup micromodule**
   - Future apps can ask bank `0`: "for my app id and resource set X, which
     items are loaded?"
   - The module should read compact records only; no string-heavy parser.
   - App impact should be opt-in and measured per app.

3. **No broad shared-app dependency**
   - Normal apps must not link registry parsers, resource-set writers, or
     service dispatch code unless they explicitly need it.

## Expected Memory Impact

Target ranges before implementation:

| Area | Expected impact if kept lean |
| --- | ---: |
| launcher | `+500` to `+1500` CODE, `0` to `+128` BSS |
| launcher_easyflash | likely lower risk because of current headroom |
| ReadyShell | `0` if launcher keeps emitting current tiny metadata |
| ReadyBASIC | `0` strongly preferred; avoid generic lookup there |
| REU Viewer | `+300` to `+1000` CODE acceptable if display improves |
| future plugin apps | opt-in micromodule cost only |

If the C64 launcher starts parsing a rich key/value language, assume the cost
will exceed this and reject the approach. Host-side validation can be richer;
C64 runtime parsing must stay positional and bounded.

## Future Work Checklist

1. Define the resource-set record layout.
2. Define item record flags and class codes.
3. Decide string/tag strategy:
   - 4-byte tags only;
   - fixed short string table;
   - source-line reference plus compact tags.
4. Extend the host catalog/manifest generator to validate the new descriptor.
5. Keep a compatibility bridge for current `rsovl+` and `rbcore+` until the
   catalogs are converted, or convert all catalogs in one branch.
6. Update disk launcher parser with a bounded positional parser.
7. Update EasyFlash generator to emit the same resource-set and item records.
8. Keep ReadyShell and ReadyBASIC app runtime contracts tiny.
9. Extend REU Viewer to display:
   - resource set name/tag;
   - class;
   - required/optional;
   - selected/loaded/failed;
   - bank ordinal and physical bank.
10. Extend checked-in REU Viewer VICE tests beyond the current QuickNotes
    owned-allocation before/after probe so they also navigate loader resource
    banks and assert owner/detail text.
11. Add a C64-side or otherwise reliable bank `0` runtime validator.
12. Add future optional plugin pilot only after the schema is proven by
    ReadyShell/ReadyBASIC parity.

## Existing Remaining Work Carried Forward

These items were still open at the end of Phase 1 and remain in scope here:

- generic arbitrary dependency loading for unknown app-specific overlays or
  modules beyond current concrete contracts;
- broader REU Viewer VICE regression coverage for visible owner/detail text;
  the QuickNotes app-owned allocation path now has a checked-in before/after
  unload probe, but loader-resource display should get equivalent assertions;
- runtime bank `0` validator for `RCB0` header/table/resource records;
- headless app/service invocation records and dispatcher;
- modal UI service invocation, including shared file open/save;
- service-temp cleanup semantics for future service flows;
- possible cleanup of historical reserved/overlapping bank `0` areas after the
  v4 layout stays stable.

## Verification Discipline

Every implementation branch must capture:

- before/after headroom for launcher, launcher_easyflash, REU Viewer, and every
  impacted app;
- ReadyBASIC headroom and behavior, even if ReadyBASIC is not supposed to link
  new code;
- ReadyShell resident and overlay memory impact;
- static schema/generator checks;
- regular ReadyBASIC VICE suites;
- regular ReadyShell VICE probe;
- full EasyFlash/cartridge VICE suites;
- focused REU Viewer navigation tests for any display/metadata change.

Do not accept a feature that saves design elegance by spending ReadyBASIC
headroom or growing the resident shim. The launcher and REU bank `0` should
own complexity; apps should opt into only the smallest needed reader.
