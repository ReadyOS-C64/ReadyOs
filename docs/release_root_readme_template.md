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

The current public `{{PUBLIC_VERSION}}` release is still comparatively generic
rather than being explicitly tailored to the new C64 Ultimate. The next release
is expected to push further in that Ultimate-first direction while still trying
to stay usable on other REU-capable C64 setups.

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

- If you are on C64 Ultimate, Ultimate 64, or VICE and want the fullest,
  easiest default: start with `precog-d81` or `precog-dual-d71`.
- If your setup wants the main local verification target and is comfortable with
  two mounted `1571` drives: use `precog-dual-d71`.
- If your setup prefers one full-content image on a `1581` / `D81` path: use
  `precog-d81`.
- If you only have `1541`-class compatibility but can mount two disks: use
  `precog-dual-d64`.
- If you can only mount one `D64` at a time, especially in simpler emulators,
  loaders, THEC64-style flows, or web environments: choose one of the solo
  `D64` subsets based on the app group you care about.

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
per-variant `help.md` / `helpme.md`.

## How To Think About The Variants

The variants are not random repacks. They are different answers to the same
question: "What is the best ReadyOS shape for this storage environment?"

- `precog-dual-d71` is the broadest "mainline" profile when two `1571`-class
  drives are available. It remains the primary local verification target.
- `precog-d81` is the cleanest single-image full-content option when `1581`
  support is available.
- `precog-dual-d64` exists because many C64-adjacent environments still top out
  at `D64`, but can at least keep two images mounted.
- The solo `D64` variants exist for the environments that cannot do more than
  one `D64` at a time. Instead of forcing a bloated or broken one-disk build,
  ReadyOS splits into intentional subsets.

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
- use `16MB` where the environment supports it
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
- Read that folder's `helpme.md` for exact boot and VICE setup details.
- Use [readyos64.com]({{MAIN_SITE_URL}}) as the public front door.
- Use [readyos.notion.site]({{WIKI_URL}}) for the more wiki-like working docs.
- Use [GitHub]({{GITHUB_URL}}) for source, issues, and future packaged releases.

ReadyOS `{{PUBLIC_VERSION}}` is still explicitly experimental, but the purpose
of this release layout is simple: make it easier to pick the right image for the
hardware or emulator in front of you, instead of assuming every C64 environment
looks the same.
