# September 2026 documentation audit

This audit follows source at `4ef8f37` and subsequent documentation changes.
Existing release binaries and generated outputs were already dirty at the start.
Historical measurements, transcripts and experiments are preserved; a current
source statement must not turn their old results into a claim about a new build.

## Completion checklist

- [x] Current Markdown, styled HTML and private documentation agree on current
  ReadyBASIC commands, disk modules, examples and important limitations.
- [x] One reference page includes every BASIC example and explains the implemented
  command inventory, distinguishing proposed commands from implemented ones.
- [x] Main and SKU readmes explain Ultimate host-image configuration and SETUP,
  app hardware/REU requirements, final PRECOG and the two future directions.
- [x] Read.Me generated C/PRG is refreshed through the normal build flow and its
  size change measured; compact help does not duplicate the large reference.
- [x] Documentation generators preserve the changes on regeneration; HTML,
  links, release ordering and relevant build checks pass.
- [x] Findings needing code, packaging or hardware verification are reported below.

## Findings needing more than a documentation edit

1. **Media packaging differs by profile.** Regular D81 and its Ultimate derived
   profile include `rbm.media`, four media assets, RBSND07 and two combined demos.
   D71, focused D64 and KFF2 retain the three sample modules and original 41
   examples; the separate rsdebug D81 has 27 examples. Documentation states this accurately;
   making all profiles include the new set requires packaging/capacity work.
2. **Media loaders address drive 8.** The new SEQ loaders use KERNAL LFN 14,
   device 8, secondary address 2. Copying the package onto the normal D71 drive-9
   image alone is not enough to make those workflows work.
3. **No general SID compatibility promise.** MUSTUNE accepts a constrained PAL
   PSID v2 format, with a reserved $9000 driver and vetted $9200 tune. Header
   validation is not proof that arbitrary player code respects the scratch,
   memory or IRQ contract. RSID and arbitrary SID collections are not supported.
4. **Existing graphics/procedure limitations.** FCIRCLE is a filled rectangle
   placeholder; FPOLY is a conservative convex fan. Media investigation records
   float FUNC/assignment and IF/EXEC workarounds that are not interpreter fixes.
5. **Ultimate speed is not yet an OS-wide service.** USPEED/UMHZ use the C64U /
   U64 Elite-II register table. They do not implement automatic safe-I/O guards
   or older U64 speed-table detection. Keep boot and disk loads at safe speed.
6. **Runtime acceptance remains separate.** Prior demo evidence includes test
   fixture limitations and historical fixed-wait hardware procedures. Current
   C64U disk loading must be observed over video until visibly complete before
   any RAM/screen read. Documentation updates cannot revalidate old test runs.
7. **Sample modules replace built-ins.** The three old diagnostic packages write
   descriptors to core-bank $1500/$1600/$1700, now occupied by built-ins. The
   loader accepts those ranges. They are intrusive developer tests, not safe
   general-purpose add-ons. Moving them into unoccupied descriptors or adding
   collision-aware registration requires code/package changes. Media's eight
   descriptors start at $1C20 and do not share this problem.
8. **ReadyShell host/C64 serialization signatures differ.** The serializer
   writes character literals: a host build emits ASCII `52 53 56 31`, while
   the cc65 C64 build emits PETSCII `D2 D3 D6 31` for `RSV1`. Readers validate
   their target's literal bytes. Documentation now distinguishes them, but
   portable interchange needs an explicit byte-format decision and code/tests.
   No runtime serialization behavior was changed in this documentation pass.

## Footprint constraint resolved

The existing 0.5L Ultimate D81 has one free block and the previous 27,272-byte
Read.Me. The first documentation revision grew to 28,381 bytes and would not
fit as a direct replacement. At the user's request, duplicated introductory
prose was tightened while retaining every section, app guide, control and new
feature topic. The final Read.Me is **26,869 bytes: 403 bytes smaller** than the
original, and **37 pages instead of 39**. It takes 106 rather than 108 disk
file blocks, removing this update's capacity problem. Regular D81 was rebuilt;
other existing SKU images, including Ultimate, were not repacked merely to
refresh their external documentation.

## Work record

- Initial inventory completed: source descriptors and media loaders inspected;
  profile differences and stale overview claims identified. Main README updated
  to communicate the current features, requirements and agreed product direction.
- Added the source-generated reference with 98 built-ins, eight media commands,
  34 developer-proof commands and 45 complete, verbatim BASIC listings. Every
  example includes its purpose, commands used, disk name and profile availability.
- Refreshed 46 release readme/help files from each existing manifest's artifact
  names; the documentation-only refresher does not relabel or rebuild images.
- Refreshed current public and private Markdown, 14 styled HTML counterparts and
  the map-backed memory diagrams. Corrected descriptor/core-bank versus
  payload/code-bank placement; old measurements remain explicitly historical.
- Built regular D81 through `/bin/bash ./run.sh --profile precog-d81 --build-only`.
  Final documentation build is 0.5P. Read.Me changed from 39 to 37 pages and
  27,272 to 26,869 bytes; generated C/header match source and all pages fit
  38 columns by 18 lines. Readback from the D81 matches the built PRG exactly.
- Browser review verified the reference introduction and media-command table.
  Disabled dollar-delimited math in the renderer so BASIC strings and hex
  addresses remain literal. Added regression checks for 99 built-in-table rows,
  nine media-table rows, 35 sample-table rows, all full listings and anchors.
- Validation passed: documentation contracts, 48 HTML status classifications,
  all reference/Read.Me generation checks, release-doc reproducibility, local
  links, five byte-verified shim HTML listings, release directory ordering, and
  exact regular-D81 ReadyBASIC runtime/module/demo/resource readback. This was
  a documentation/build audit, not a new physical-hardware acceptance run.
- Corrected ASCII/PETSCII descriptions in QuickNotes, clipboard bundles,
  Simple Cells, CAL26, Dizzy and ReadyShell format documentation. Checked
  the stored signatures against authoritative seed files and C64 source;
  also corrected Simple Cells' documented binary version from 1 to 2.
  Legitimate host ASCII and ASCII-compatible digit descriptions remain.

## Requirement-to-evidence map

| Requested outcome | Current evidence |
| --- | --- |
| Current public/private documentation without losing history | Updated canonical ReadyBASIC guides, private MEMORY_MAP/SHIM_PLAN/current-state guide, explicit historical measurement labels; existing example files untouched |
| All examples and command explanations on one page | `verify_readybasic_documentation.py` checks all 45 verbatim source listings, descriptor-derived built-ins, rendered media/sample tables and every in-page anchor; examples have individual purpose descriptions |
| Styled HTML stays aligned with Markdown | `render_current_docs.py --check` verifies 14 counterparts; browser inspection confirmed readable introduction/table and literal BASIC dollar signs |
| Main/SKU setup, app requirements and future plans | Main README, app-requirements guide, shared profile help generator, EasyFlash help generator and root release template; `refresh_release_docs.py --check` checks all 46 generated files |
| Small generated Read.Me update | Normal D81 build, exact generated C/header check, 37 correctly bounded pages, 403 bytes smaller than original, byte-identical PRG readback from rebuilt D81 |
| Consistency and remaining non-doc issues | 315 current/tracked/new documents and 289 local links checked, 48 HTML classifications, five shim listings, every release directory order; eight findings above remain deliberately unfixed |

Private documentation remains in its existing gitignored location. This audit
updates it locally and checks its current links; it does not publish private
material or change ignore rules. No source runtime behavior, sample BASIC
program, hardware configuration, or full Ultimate image was changed by this
documentation pass.
