# ReadyOS {{PUBLIC_VERSION}} Release Guide

ReadyOS PRECOG is an experimental REU-first environment for a modern Commodore
64 setup. Its long-term center of gravity is the new Commodore 64 Ultimate and
related Ultimate-family hardware, but it is intended to support a wide range of
C64 setups that have a reasonably large REU. This release line is organized as
multiple disk-image variants so the same ReadyOS runtime can fit different
real-world C64 environments without pretending every machine, cartridge,
loader, or emulator mounts the same media.

- Public release line: `{{PUBLIC_VERSION}}`
- Current artifact build in this tree: `{{VERSION_TEXT}}`
- Main site: [readyos64.com]({{MAIN_SITE_URL}})
- Wiki / working knowledge base: [readyos.notion.site]({{WIKI_URL}})
- GitHub source and issues: [ReadyOS-C64/ReadyOs]({{GITHUB_URL}})

If this folder is distributed as a GitHub release or a packaged download, this
README is the landing page for the whole release line. The profile folders next
to it are the actual ReadyOS SKUs for different disk and drive constraints.

The shared ReadyOS runtime remains broadly usable on REU-capable C64 setups.
Release `{{PUBLIC_VERSION}}` also begins the explicit new-Commodore-64-Ultimate
path through a dedicated SKU, while retaining portable disk-based variants for
other environments.

The dedicated `precog-ultimate` D81 compiles and enables the C64 Ultimate DOS
DMA launcher and includes a standalone first-run SETUP utility. Other public
profiles use the portable disk path. The Ultimate path always retains normal
disk fallback when UCI, the image, or a transfer is unavailable.

## What ReadyOS Is

ReadyOS is not trying to be a generic desktop shell squeezed into a C64. It is
an opinionated, keyboard-first environment built around the idea that a modern
C64 workflow should feel immediate once the machine is up:

- fast app switching instead of cold-starting every tool from BASIC
- suspend and resume across apps instead of constantly reloading state
- shared clipboard and common interaction patterns across tools
- full-screen, terminal-style apps that favor repeatable keyboard workflows
- REU-backed state so the machine can behave more like a ready workspace than a
  single-program-at-a-time disk menu

The current public release line is `{{PUBLIC_VERSION}}`. Full-content ReadyOS
profiles currently expose `{{CURRENT_APP_COUNT}}` launcher-visible apps, with
the exact app mix depending on the variant you choose.

## Runtime And REU Contract

- The active app snapshot is `$1000-$C5FF` (`$B600` bytes). The resident 1 KB
  shim owns `$C600-$C9FF`; its public ABI remains at `$C800-$C9FF`.
- Physical REU bank `Skip` is the **ReadyOS bank** and is never allocated to an
  app/resource. Physical `Skip+1` is the first dynamic bank.
- The ReadyOS bank contains the launcher snapshot at `$0000-$B5FF` and
  schema-v5 mappings, status, clipboard, hotkeys, registry/catalog, audit, and
  launcher runtime state at `$B600-$FFFF`.
- Logical app tokens resolve through the explicit `$B740` table; they are not
  physical bank numbers and must not be converted with arithmetic.
- ReadyShell and ReadyBASIC resource banks are loader-assigned and recorded in
  the ReadyOS bank. ReadyBASIC retains its custom ca65/ld65 compact-image shape.

## Why There Are Multiple Variants

The short answer is that C64 storage and loader realities are not uniform.

ReadyOS wants to run on:

- C64 Ultimate and Ultimate 64 setups
- VICE on modern desktops
- real C64 hardware with REU-capable cartridges or other REU-capable expansions
- THEC64 Mini and Maxi style workflows
- web C64 emulators and simplified loaders that may only mount a single `D64`

Those environments differ in three important ways:

1. Drive type support.
   Some setups are happy with `1571` or `1581` style media, while others are
   effectively limited to `1541` / `D64`.

2. Number of simultaneously mounted images.
   Some environments can keep two drives online all the time. Others can only
   mount one disk image at once, which forces ReadyOS into smaller curated
   subsets.

3. REU path and convenience model.
   ReadyOS is designed for an REU-capable path. VICE can emulate that cleanly,
   Ultimate-family hardware can provide it directly, and some other modern
   setups can approximate it well enough to be practical. But the storage SKU
   still has to match the drive and media constraints of the environment.

That is why this release line ships multiple folders instead of pretending one
image is universally correct. The runtime philosophy is shared. The disk-image
packaging changes to match the target.

## Quick Recommendation By Environment

- If you are on the new Commodore 64 Ultimate and want Ultimate DOS DMA plus
  guided first-run path setup, use `precog-ultimate`.
- If you are on Ultimate 64, VICE, or another 1581-capable setup and want the
  fullest portable default, use `precog-d81`. It remains the main ReadyOS SKU
  and default build/run target.
- If your setup uses two `1571` drives, use `precog-dual-d71`. Its two-image
  boot set can be supplemented after boot by swapping the optional app-data
  D71 into drive `9`.
- If you only have `1541`-class compatibility but can mount two disks: use
  `precog-dual-d64`.
- If you can only mount one `D64` at a time, especially in simpler emulators,
  loaders, THEC64-style flows, or web environments: choose one of the solo
  `D64` subsets based on the app group you care about.
- If that one-D64 environment is specifically for ReadyBASIC, choose
  `precog-solo-d64-readybasic`; it includes ReadyOS, ReadyBASIC, all module
  packages, and the complete example set on one image.

## Public Variant Matrix

This release line currently has `{{PUBLIC_VARIANT_COUNT}}` public variants.

{{PUBLIC_VARIANT_MATRIX}}

## What The Release Root Contains

The release line is centered on these public folders:

{{PUBLIC_VARIANT_FOLDERS}}

Depending on whether a local workflow built one profile or all profiles, a
working tree may not contain every folder until the full multi-profile build has
been run. The intended GitHub release layout is this shared root README plus the
variant folders that carry the actual images, boot PRGs, `manifest.json`, and
per-variant `README.md` plus compatibility copies in `help.md` / `helpme.md`.

## How To Think About The Variants

The variants are not random repacks. They are different answers to the same
question: "What is the best ReadyOS shape for this storage environment?"

- `precog-d81` is the mainline, recommended profile and the cleanest
  single-image full-content option when `1581` support is available.
- `precog-dual-d71` is the 1571-oriented alternative. Its two boot disks keep
  the core app set and ReadyBASIC modules online; its optional third disk is a
  post-boot drive-9 swap for lesser apps and all ReadyBASIC examples.
- `precog-dual-d64` exists because many C64-adjacent environments still top out
  at `D64`, but can at least keep two images mounted.
- The solo `D64` variants exist for the environments that cannot do more than
  one `D64` at a time. Instead of forcing a bloated or broken one-disk build,
ReadyOS splits into intentional subsets.
- `precog-solo-d64-readybasic` is the exception to the subset split: the full
  ReadyBASIC runtime, modules, and examples fit together on one focused D64.

## Disk Directory Order

Every D64, D71, and D81 applies the same semantic ordering to the files it
contains: boot-chain PRGs (`PREBOOT` first, followed by any `SETD71` / `SHOWCFG`,
then `BOOT` and `LAUNCHER`); configs; ordinary SEQ/USR data; main app PRGs;
overlays/modules; REL data; and finally any ReadyBASIC examples. This keeps
`LOAD"*",8` safe on bootable images while allowing each SKU and disk side to
omit categories it does not need. The EasyFlash CRT uses cartridge banks
instead; its companion D64 follows the applicable data-file ordering.

That last category matters more than it may seem. A web emulator that only
mounts one `D64` is a very different target from VICE on a desktop with REU and
multiple virtual drives. THEC64 Mini / Maxi style workflows can also be more
pleasant with smaller, direct, single-image choices. A release that pretends all
of those paths are identical would be harder to understand and harder to boot.

## REU Expectation

ReadyOS is still an REU-first environment. The disk-image variants solve storage
shape, not the absence of an REU-capable path.

Recommended baseline:

- enable the REU
- use at least `1MB`; use `8MB` or `16MB` where the environment supports it
- treat VICE and Ultimate-family hardware as the smoothest targets today

On real C64 hardware, the exact cartridge or expansion path can vary. The main
question is not the brand of REU-capable device, but whether the setup can
deliver the REU behavior the runtime expects and whether the chosen media SKU
matches the drive constraints of that setup.

## Debug Variants

{{DEBUG_VARIANT_NOTE}}

If you are just trying to run ReadyOS, prefer the non-debug variants first.

## Where To Go Next

- Start with the variant folder that matches your environment.
- Read that folder's `README.md` for exact boot and VICE setup details;
  `help.md` and `helpme.md` carry the same content for existing workflows.
- Use [readyos64.com]({{MAIN_SITE_URL}}) as the public front door.
- Use [readyos.notion.site]({{WIKI_URL}}) for the more wiki-like working docs.
- Use [GitHub]({{GITHUB_URL}}) for source, issues, and future packaged releases.

ReadyOS `{{PUBLIC_VERSION}}` is still explicitly experimental, but the purpose
of this release layout is simple: make it easier to pick the right image for the
hardware or emulator in front of you, instead of assuming every C64 environment
looks the same.
