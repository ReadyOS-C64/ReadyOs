# Requirement-by-Requirement Completion Audit

This checklist is intentionally evidence-based. An item becomes complete only
after the named final-state evidence exists and has been inspected.

| Requirement | Final evidence | Status |
|---|---|---|
| Full resident 1 KB shim at `$C600-$C9FF` | linker configs, boot/EasyFlash images, `verify_readyos_shim.py`, binary identity | proven |
| `$C600-$C7FF` remains shim headroom, not app RAM | all app linker configs/maps, ReadyBASIC custom layout, memory verifier | proven |
| Stable public ABI remains `$C800-$C9FF` | canonical shim source and symbol/byte verification | proven |
| App snapshot is `$1000-$C5FF` / `$B600` | shim opcodes, boot/loaders, app configs, resume verifier | proven |
| Banks below `Skip` are skipped; physical `Skip` is ReadyOS; `Skip+1` is first dynamic | allocator/control-bank/boot/EasyFlash source plus schema dumps | proven |
| App/map/status/clipboard/catalog authority is in ReadyOS bank | schema-v5 offsets and callers; launcher/schema UI tests | proven |
| Disk, EasyFlash, and Ultimate DMA paths agree | regular/EasyFlash suites and Ultimate hardware acceptance | proven |
| ReadyBASIC custom assembler/linker shape stays compatible and tight | source contract, linker map, shape verifier, full regular/EasyFlash suites | proven |
| ReadyShell memory and overlay ABI stay tight | map/report, host suite, regular/EasyFlash cross-app suites, Ultimate sequence | proven |
| Clipboard and app resume state survive switching | regular non-empty clipboard plan plus cross-app suites | proven |
| All release/SKU builds succeed | final `/bin/bash ./run.sh --build-all` | proven |
| All VICE UI suites succeed | retained full regular and EasyFlash aggregate manifests plus final targeted clipboard run | proven |
| Ultimate DMA loading succeeds | editor direct/return, F3 selection, F1 all, manifest, ReadyBASIC, ReadyShell, ReadyIRC | proven |
| Linker/BSS/heap/overlay/headroom analyzed before and after | JSON/Markdown comparison and regenerated reports | proven |
| Main, SKU, app, public/private Markdown and HTML are current or visibly historical | documentation verifier, HTML classification, corpus scan | proven |
| Every full commented HTML shim source matches current source | five-listing byte/directive verifier | proven |
| No files or retained historical artifacts are deleted | final Git deletion audit and release-artifact restoration | proven |
| Working changes committed in reviewable scopes | clean diff checks and Git commits | pending |
