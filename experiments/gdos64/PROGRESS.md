# GDOS bridge experiment progress

2026-09-26: branch `codex/gdos64-bridge-experiment`, based on ReadyOS 0.5-rc2
at beb9dd3. Existing C64OS demo changes and unrelated generated artifacts are
not part of this experiment and must not be discarded or committed with it.

Original lab backed up to sibling backups/GDOS64-pre-bridge-20260926-195800.tar.gz
(SHA256 d77e2ac2b00dac5c4addc588601a4159f2548735f9f2e6a3402ac997a00e0566).
Original lab remains untouched; native accessory project is ../gdos64-bridge.

Implemented provisional ReadyOS catalog entry only for precog-d81. The rsdebug
profile previously shared its catalog and now has its own unchanged copy.
Normal skip remains 0; experiment apps.ini uses skip71. Build-time validators
were extended to71; shim ABI unchanged. Full builds use normal run.sh.

First build and ReadyOS destructive full-RAM proof passed. Cold entry into
GDOS reaches the native GeoDesk desktop with 64-bank DACC on drive9. Warp
quickly triggers the screensaver; automated waits must stop warp as soon as
native desktop is rendered. GDOS menu Applications / DeskAccessories finds
ReadyOS Bridge on the system disk.

First CVT packaging had two oversized Python slice replacements (29-byte
signature into28-byte slice,39-byte description into38). That corrupted file
header/load addresses. Fixed exact slice sizes and assert both header blocks
remain254 bytes. Private VICE restarted; originals unaffected.

Cold and warm native round trips now work with ReadyOS Editor and geoWrite.
REU ownership checks passed: GDOS banks 0–63 remain byte-identical while ReadyOS
works; ReadyOS application banks 72–255 remain byte-identical while GDOS works.

Native caller-state investigation: GDOS's own GD.GEOHELP.DA explicitly preserves
all 417 bytes of dlgBoxRamBuf before nested dialogs. Our accessory now follows
that pattern before calling RstrAppl. Otherwise its confirmation dialog replaces
the saved caller vectors/graphics state. Removed GEOS_InitSystem from preparation;
only native input release and C1581 PurgeTurbo handoff remain. The bitmap can look
restored before native application restore has finished, so automation respects
input-queue backpressure. A two-pixel comparison difference was the text cursor
on the next line; the line comparison now excludes that cursor.

Automation input fix: one-shot writes to mouseData race UpdateMouse. The private
helper now waits for real execution to reach GDOS 1.03's ExecMseKeyb ($FD68),
then enqueues each mouse event. These temporary checkpoints do not change code
or the program counter. The release event must preserve GDOS's pointer position:
menus clamp it, and moving it back to the original button closes the menu.
A port-simulation attempt was abandoned; the normal 1351 device remains selected.

The normal regular D81 was rebuilt with skip 0 after caching the private skip71
disk. Main-catalog inclusion remains limited to precog-d81. Release ordering and
dynamic-launcher checks passed. The recorded full suite passed all 14 checks, including continued geoWrite input
after the second round trip. `../gdos64-bridge/evidence/video/readyos-gdos64-roundtrip.mp4`
is a continuous 109.7-second H.264 capture at 1152x816/20 fps; full decode passed.
`roundtrip-report.json` and `demo-roundtrip-report.json` contain the passing suite.

The stock skip-zero guard passed through a normal ReadyOS boot. Editing boot
device 9 to 8, saving with readback, leaving/reopening the app and reloading the
saved setting all passed; the private config was restored to device 9 afterward.
See `stock-guard-report.json`. The regular release remains skip zero.

Native mouse automation ultimately queues only the press at ExecMseKeyb and
lets the 1351 driver observe the real released button. No synthetic release is
written after a command that might already have switched into ReadyOS.

No commits or pushes made. Existing unrelated C64OS notes and files preserved.
The build-only CAR timestamp/CRC churn was removed from the four files that
were clean at task start; no C64OS code was changed by this experiment.

Final checks: memory-map verification and REU control-bank schema-v5 checks passed.
Private experimental VICE is left at the ReadyOS launcher, skip 71, speed 100.
