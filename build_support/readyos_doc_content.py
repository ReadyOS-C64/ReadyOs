"""Shared editorial facts for release documentation (not runtime metadata)."""

PRODUCT_DIRECTION = (
    "PRECOG 0.5 is planned as the final PRECOG release: the series that established "
    "what is possible and clarified the vision. Next comes ReadyOS Ultimate, the "
    "main focus, installing/configuring real files and folders on Ultimate storage "
    "and using its hardware features; and ReadyOS Universal, continuing disk-image "
    "and REU workflows for VICE, THEC64 and original hardware. Universal effort will "
    "follow user interest and demand. These are future directions, not separate "
    "products shipped here. Standalone releases of many apps are also planned, "
    "including both apps with their own REU needs and apps without them. We remain "
    "committed to standalone ReadyBASIC independent of Ultimate and ReadyOS; "
    "this does not promise a no-REU interpreter. Current ReadyOS app PRGs still "
    "require the ReadyOS runtime."
)

ULTIMATE_PATH = (
    "ReadyOS cannot discover which Ultimate host folder/image it was booted from: "
    "the mounted C64 drive does not provide the enclosing D81 host pathname. "
    "Ultimate DOS fast loading therefore needs the D81's absolute host path in "
    "`[launcher] c64u_image_path` in `apps.cfg`, together with `dma_loading=1`. "
    "SETUP browses Ultimate storage, validates the selected D81, and safely saves "
    "those settings inside it. Run SETUP again after moving or renaming the image. "
    "The normal disk loader remains the fallback."
)

APP_REQUIREMENTS = {
    "quicknotes": "portable; own REU note storage",
    "readyshell": "portable; own REU overlays/state",
    "readybasic": "portable core; own REU core/code and buffers; USPEED/UMHZ require Ultimate",
    "readyirc": "Ultimate-only TCP; own REU scrollback",
    "uzip": "Ultimate-only DOS; own REU package/workspace",
    "ucitest": "Ultimate-only command lab; no separate app-owned REU workspace",
    "clipmgr": "portable; shared REU clipboard/history",
    "reuviewer": "portable; inspects shared REU allocation records",
    "sysinfo": "portable diagnostics; Ultimate details available only on Ultimate",
    "setup": "Ultimate-only standalone setup; checks REU; not a ReadyOS app",
}

def app_requirement(name):
    return APP_REQUIREMENTS.get(name, "portable; no separate app-owned REU workspace")

def basic_inventory(profile):
    entries = [c for d in profile.get('disks', []) for c in d['contents']]
    examples = [c for c in entries if c['type'] == 'prg' and
                (c['name'].startswith(('rbgfx', 'rbugfx', 'rbsnd')) or
                 c['name'] in {'rbtest1', 'rbproc1', 'rbprocerr'})]
    packages = [c['name'] for c in entries if c['name'].startswith('rbm.')]
    if not examples and not packages:
        return []
    lines = ["", "## ReadyBASIC examples and modules", "",
             f"This profile defines {len(examples)} BASIC example/test PRGs and "
             f"{len(packages)} disk module packages: " + ', '.join(f'`{p}`' for p in packages) + ".",
             "Built-in graphics, immediate SID sound, MEMCAP and BORDER need no disk-module load. "
             "USPEED/UMHZ are built-in but require compatible Ultimate software turbo registers."]
    if 'rbm.media' in packages:
        lines += ["The new set includes RBSND07, RBGFXSNDDEMO and RBUGFXSNDDEMO. "
                  "Use `LDMOD(\"RBM.MEDIA\",M%)` for eight on-demand music/image/sprite commands. "
                  "Reserve with `MEMCAP(36864)` before strings/music; load images before starting music. "
                  "The vetted PSID player is PAL-only. The standard demo avoids Ultimate speed calls; "
                  "the Ultimate demo requires C64U Turbo Registers and uses 1 MHz during disk I/O.",
                  "In Orbital Echoes, Space restores the cached background, Q stops/releases music, "
                  "and M keeps music playing at the text prompt. After M use "
                  "`MUSDROP():CLR:MEMCAP(40960)` to release it."]
    else:
        lines += ["The example count above is this profile's actual configured subset. The new `rbm.media` "
                  "package, tune/images and combined demos are currently packaged in regular D81 "
                  "and Ultimate D81 only. They are not implied by having the ReadyBASIC runtime."]
    lines += ["The new disk-module/resource loaders read drive 8; they do not take a device argument. "
              "See the repository's `docs/readybasic_reference.md` (and HTML counterpart) for "
              "every example, command contracts and exact per-profile availability."]
    return lines
