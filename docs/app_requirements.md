# App hardware and REU requirements

Every current ReadyOS app runs within an REU-backed system: launcher snapshots,
switching and shared services already need REU. The table separates that common
requirement from an app's own code or data resources. Inclusion in a portable
disk image does not make an Ultimate-dependent application portable.

| App | Ultimate required for its main function? | REU beyond the app snapshot |
| --- | --- | --- |
| Editor | No | Shared clipboard when used; no separate editor workspace |
| QuickNotes | No | Own REU-backed note banks |
| Calc Plus | No | Shared clipboard when used; no separate calculator workspace |
| Hex Viewer | No | No separate app workspace |
| Clipboard Manager | No | Shared clipboard payload/history is the data being managed |
| REU Viewer | No | Reads ReadyOS bank ownership/allocation records; no separate data workspace |
| System Info | No | Probes/reports REU; Ultimate-specific fields are conditional |
| Task List | No | No separate app workspace; shared services still apply |
| Simple Files | No | No separate app workspace |
| Simple Cells | No | No separate app workspace |
| 2048 | No | No separate app workspace |
| Sidetris | No | No separate app workspace |
| Calendar 26 | No | No separate app workspace; REL-backed disk data |
| Dizzy | No | No separate app workspace; REL-backed disk data |
| Deminer | No | No separate app workspace |
| Read.Me | No | No separate app workspace; content is compiled from local Markdown |
| ReadyShell | No | Loader-assigned overlay and state/scratch resource banks |
| ReadyBASIC | No for core and standard examples | Loader-assigned core/code banks and optional handle-based buffers, surfaces, lists and tile resources |
| ReadyIRC | Yes, UCI TCP | App-owned REU scrollback |
| UCI Tester | Yes, for UCI experiments | No separate app-owned workspace |
| Ultimate Zip | Yes, Ultimate DOS | Launcher package resource plus REU work/data resources |
| C64OS bridge | Dedicated Ultimate image available | 16 MiB REU, skip 39, C64OS 1.09 capped at 8–32 banks |
| GDOS64 bridge | Dedicated Ultimate image available | 16 MiB REU, skip 71, GDOS64 1.03 with 4 MiB DACC |
| GEOS 2.0 bridge | Portable and Ultimate bridge images available | 16 MiB REU, skip 71, stock GEOS 2.0 |
| SETUP | Yes, Ultimate DOS | Standalone utility that checks REU; no ReadyOS snapshot/shim dependency |

ReadyBASIC's `USPEED`, `UMHZ` and `RBUGFXSNDDEMO` need C64 Ultimate / U64
Elite-II software speed registers. That does not make the interpreter or the
standard `RBGFXSNDDEMO` Ultimate-only. The vetted media music player is PAL-only.

## Planned standalone releases

Many apps are intended for future standalone release: both those whose own
work needs REU and those whose own work does not. Existing ReadyOS PRGs are not
therefore standalone already; packaging, startup and shared-service integration
still need work. Standalone ReadyBASIC independent of Ultimate and ReadyOS is a
specific commitment, while a no-REU interpreter is not promised.

PRECOG 0.5 is planned as the final exploratory release. ReadyOS Ultimate will
focus on installation/configuration in real Ultimate files/folders and hardware
features. ReadyOS Universal will continue disk-image/REU workflows for VICE,
THEC64 and original hardware, with investment following user demand. These are
future directions, not changes to today's app ABI.
