# Bridge sources and release boundaries

All authored bridge source needed to build the release payloads is in this repo:

| Component | Source | Checked-in binary |
| --- | --- | --- |
| ReadyOS C64OS app | `src/apps/c64os`, `experiments/c64os` | `bin/c64os.prg` |
| C64OS native return utility | `experiments/c64os/readyos_utility.s` | `bin/c64os-readyos-utility.prg` and the CAR |
| C64OS native Bridge Setup / patch / undo / installer | `experiments/c64os/setup/src`, `setup/tools` | `assets/c64os/bridge.car` |
| ReadyOS GDOS app and transport | `src/apps/gdos64`, `experiments/gdos64` | `bin/gdos64.prg` |
| Native GDOS return accessory | `experiments/gdos64/accessory.s` | `assets/gdos64/readyos-bridge.cvt` |
| ReadyOS stock GEOS 2.0 app and transport | `src/apps/geos20`, `experiments/geos20`, shared GDOS transport | `bin/geos20.prg` |
| Native GEOS 2.0 return accessory | `experiments/geos20/accessory.s` | `assets/geos20/readyos-bridge.cvt` |

Build everything through `/bin/bash ./run.sh --build-all --for-release` for the
public `0.5` version without a rolling letter. Native CAR/CVT builds use these
checked-in sources, without requiring sibling source folders or licensed OS disks.
CAR/CVT companions are copied beside the applicable release images, not into
those images. These companions do not contain the licensed operating systems.

Normal D81 includes the optional C64OS app manifest/CAR and the GDOS catalog app,
but its compiled skip zero disables both transitions until built with a matching
layout. C64OS uses skip39 plus its 1.09-only bank cap; GDOS and stock GEOS use
skip71 with different native signatures and allocation checks. Stock Ultimate,
KFF2, smaller disk SKUs and EasyFlash exclude all bridge apps. Dedicated
Ultimate GDOS/GEOS profiles are experimental, not additions to stock Ultimate.

The user reports manual testing of the hardware. Earlier automated reports
separately demonstrate GDOS and GEOS transitions in VICE, including live geoWrite
state, continued typing and reciprocal REU preservation. A new full hardware
regression has not been performed as part of this release integration.

Useful test/recording tools and fixtures are retained under each experiment.
`experiments/geos_common` contains their authored VICE monitor helpers. Runtime
media, evidence, recordings and licensed OS installations remain in the sibling
labs (`gdos64-bridge`, `GEOS20`, `c64os-readyos-experiment`). Launchers use those
owned runtime folders and separate emulator ports. C64OS setup host tests require
py65 plus the private original library fixture; the native UI test requires its
owned VICE/HDD environment. Ultimate capture helpers additionally use the sibling
agenticdevharness .NET client. None of these test dependencies is needed to build
the release bridges. Avoid copying test logs, live images or inventories to Git.

The old sibling bridge/setup sources are retained as historical copies. The
sources here are authoritative for release builds. Do not assume that editing a
sibling copy changes a release payload.
