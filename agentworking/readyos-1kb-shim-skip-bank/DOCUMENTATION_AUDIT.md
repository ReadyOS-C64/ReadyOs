# Documentation Audit: 1 KB Shim And ReadyOS-at-Skip Contract

## Scope

The audit covers Markdown and HTML under `docs/`, `privatedocs/`, the 0.2.5
release/SKU tree, application documentation under `src/apps/`, and top-level
Markdown. The corpus-wide scan examined 196 Markdown/HTML files before this
evidence file was added.

## Current contract checked

- Active app RAM and snapshot: `$1000-$C5FF` (`$B600`, 46,592 bytes).
- Full resident shim: `$C600-$C9FF` (1 KB).
- Shim expansion reserve: `$C600-$C7FF`; stable public ABI: `$C800-$C9FF`.
- Physical banks below `Skip`: skipped/reserved.
- Physical `Skip`: combined ReadyOS bank.
- Physical `Skip+1`: first dynamic allocation candidate.
- ReadyOS bank `$0000-$B5FF`: launcher snapshot.
- ReadyOS bank `$B600-$FFFF`: schema v5 and its maps, status, clipboard,
  hotkeys, registries, catalog, audit, and launcher runtime state.
- ReadyBASIC custom assembler/linker shape remains below `$C600`.
- ReadyShell overlay execution remains `$8E00-$C5FF`.

## Classification and preservation

- All 38 HTML documents are explicitly classified as current or preserved
  historical/design material by `update_documentation_html_status.py`.
- Historical material was not deleted or silently normalized. Thirty-one
  retained Markdown/HTML reports carry the current-contract supersession
  marker while keeping their dated layouts and measurements.
- Current documents with legitimate uses of strings such as `$B800` (for
  example, a ReadyBASIC submodule address) were not incorrectly rewritten as
  though every occurrence described the old app-window size.
- Generated SKU `help.md`, `helpme.md`, manifests, release README/template, and
  the special 1 MB Kung Fu Flash 2 budget explanation were regenerated.

## Full shim source copies

`verify_shim_html_source.py` finds every full commented shim listing in the
HTML corpus and compares its `.byte` directives with
`src/boot/readyos_shim.inc`. Five listings pass, each containing the same 175
canonical directives and current schema-v5 token lookup annotations.

## Automated evidence

- `python3 build_support/verify_documentation_contract.py`: PASS.
- `python3 build_support/update_documentation_html_status.py --check`: PASS,
  38 classified HTML documents.
- `python3 build_support/verify_shim_html_source.py`: PASS, five complete
  canonical listings.
- `python3 build_support/annotate_historical_memory_contracts.py`: repeat-safe;
  a final rerun must report zero newly annotated documents.
