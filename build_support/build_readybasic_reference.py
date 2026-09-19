#!/usr/bin/env python3
"""Build the user reference from editorial intro, descriptors and every example."""
import argparse
import re
from pathlib import Path
import readyos_profiles as profiles
from build_readybasic_disk_modules import rbm3_payload_commands, sample_inventory

ROOT = Path(__file__).resolve().parents[1]
APP = ROOT / "src/apps/readybasic"

GFX_TOPICS = [
    "Switch/query all five graphics modes, then restore text.",
    "Plot a hires grid and diagonal, waiting for a key before exit.",
    "Draw intersecting hires line fans and restore text after a key.",
    "Compare rectangle outlines with filled boxes.",
    "Read a plotted pixel, its neighbor, then the cleared pixel with PNT.",
    "Early surface-handle probe; it predates capture/blit and does not initialize its new surface.",
    "Draw multicolor lines and pixels using the immediate primitive color encoding.",
    "Use plot/rectangle primitives on character cells in tile mode.",
    "Configure two generated sprites, position them and change color.",
    "Overlap sprites and inspect collision state; retained polling probe.",
    "Poll joystick and keyboard values without an IRQ input sampler.",
    "Combine hires lines, boxes, sprite positioning and pixel readback.",
    "Construct sprite art row by row and step through positions/colors with keys.",
    "Compare circle outline with the current rectangular FCIRCLE placeholder.",
    "Write visible character/color tiles using TILE and CHARAT.",
    "Step through sprite expansion, multicolor, priority and color controls.",
    "Outline coordinate-pair polygons held in BASIC integer arrays.",
    "Fill convex polygons held in BASIC integer arrays.",
    "Allocate REU point buffers, set indexed vertices and draw outlines.",
    "Fill REU-backed polygons alongside lines and rectangles.",
    "Exercise immediate primitives and color slots in multicolor bitmap mode.",
    "Check that PNT returns multicolor pixel slot codes 1, 2 and 3.",
    "Record plot/line/rectangle/box commands and replay a retained display list.",
    "Build a custom charset, tileset and tilemap, then render them together.",
    "Change multicolor cell palette attributes independently of bitmap drawing.",
    "Compare visible primitives and PNT across hires, multicolor and tile modes.",
    "Probe surface selection, capture/blit and error reporting before visible drawing.",
    "Draw and read back ordinary visible tile cells.",
    "Draw and read back multicolor tile cells.",
    "Replay a retained display list in multicolor bitmap mode.",
    "Capture a visible hires scene into REU, clear it and restore it with a key.",
    "Draw a polygon outline and a convex filled polygon from integer arrays.",
]
SOUND_TOPICS = [
    "Hear separate triangle, saw, pulse and noise tones using immediate SID control.",
    "Change pulse width and envelope, gating voices explicitly.",
    "Play a chromatic scale with the PAL pitch table.",
    "Compare low-pass, band-pass and high-pass SID filtering.",
    "Set frequency, waveform and packed envelopes together with VOICE.",
    "Combine three voices into a chord, then a bass/high-voice pair.",
    "Load the media module and vetted PSID; play, halt, restart and release its arena.",
    "Historical single-scene clock-paced music/graphics demo; use the newer combined demos.",
]

def example_topic(stem):
    if re.match(r'rbgfx\d\d_', stem):
        return GFX_TOPICS[int(stem[5:7]) - 1]
    if re.match(r'rbsnd\d\d_', stem):
        return SOUND_TOPICS[int(stem[5:7]) - 1]
    return {
        'rbtest1': 'Small command-expression, integer-output and uppercase-string smoke test.',
        'rbproc1': 'Structured routine/function and typed-result regression examples, including nested expressions.',
        'rbprocerr': 'Intentional negative cases; run individual numbered sections to test argument/type/stack errors.',
        'rbgfxsnddemo': 'Orbital Echoes: two cached scenes, READY sprites, palette-safe lines and PAL music; no Ultimate speed calls.',
        'rbugfxsnddemo': 'Ultimate Orbital Echoes: the same media lifecycle, with safe-speed disk loading and 64 MHz computation/animation.',
    }[stem]

# Forms are public syntax, not internal signature IDs. Proof commands use the
# descriptor's name and the examples as their executable usage reference.
CONTRACTS = {
    "ECHO1": "ECHO1(P%) — return the integer proof value.",
    "ADD16": "ADD16(A,B,R%) or R%=ADD16(A,B) — add integer arguments.",
    "UPPER": "UPPER(S$,T$) or T$=UPPER(S$) — uppercase a bounded string.",
    "LOWER": "LOWER(S$,T$) or T$=LOWER(S$) — lowercase a bounded string.",
    "HIDDENRAM": "HIDDENRAM(S$,R%) — exercise string input in the under-ROM worker.",
    "SUMNUMARRAY": "SUMNUMARRAY(A%(0),N,R%) — sum N integer elements.",
    "RANGENUMARRAY": "RANGENUMARRAY(START,COUNT,A%(0)) — write an integer range into an array.",
    "BUFMAKE": "BUFMAKE(BYTES,H%) — allocate a persistent REU buffer handle, rounded to pages.",
    "BUFFILL": "BUFFILL(H%,BYTE) — fill a REU buffer.",
    "BUFDROP": "BUFDROP(H%) — release a persistent resource handle.",
    "TEMPSCRATCH": "TEMPSCRATCH(BYTES,R%) — allocate/free temporary workspace and return its page count.",
    "FAIL": "FAIL(CODE,R%) — deliberately raise a ReadyBASIC error to test output clearing.",
    "MEMAVL": "MEMAVL() — report live BASIC free memory.",
    "SCRCAP": "SCRCAP(H%) — capture text and color as a typed REU screen handle.",
    "SCRPUT": "SCRPUT(H%) — restore a captured text/color screen.",
    "FADD": "FADD(A,B,R) or R=FADD(A,B) — BASIC floating-point addition.",
    "PAUSE": "PAUSE(UNITS) — busy-loop delay using the low byte; duration depends on CPU speed.",
    "ERRCODE": "ERRCODE() or ERRCODE(E%) — last ReadyBASIC runtime error code.",
    "ERRLINE": "ERRLINE() or ERRLINE(L%) — last ReadyBASIC error line, zero for direct mode.",
    "CPYRST": "CPYRST() — reset overlay-copy instrumentation.",
    "COPY": "COPY() — inspect overlay-copy instrumentation.",
    "LDMOD": "LDMOD(NAME$,N%) — stream/register a SEQ command package; N% receives descriptor count.",
    "GFXMODE": "GFXMODE(MODE$), M%=GFXMODE() — set/query TEXT, HIRES, MBITMAP, TILE or MTILE.",
    "GFXTEXT": "GFXTEXT() — restore normal text display.",
    "GFXCLEAR": "GFXCLEAR(C) — clear visible screen/color and applicable bitmap memory.",
    "GFXSURF": "H%=GFXSURF(MODE$) — allocate a typed REU graphics surface.",
    "GFXTGT": "GFXTGT(H%) — select surface for GFXSYNC; zero selects visible target. Primitives still draw visible RAM.",
    "GFXBLIT": "GFXBLIT(H%) — restore bitmap/screen/color from REU to visible graphics RAM.",
    "GFXSYNC": "GFXSYNC() — capture visible bitmap/screen/color into the selected REU surface; no dirty-rectangle queue.",
    "PLOT": "PLOT(X,Y,C) — set a hires bit, multicolor pair or tile cell according to mode.",
    "PNT": "PNT(X,Y,P%) — read a hires bit, multicolor pair code (not resolved color), or cell.",
    "LINE": "LINE(X1,Y1,X2,Y2,C) — draw a line with immediate primitive color semantics.",
    "RECT": "RECT(X1,Y1,X2,Y2,C) — draw a rectangle outline.",
    "FBOX": "FBOX(X1,Y1,X2,Y2,C) — fill a rectangle.",
    "CIRCLE": "CIRCLE(X,Y,R,C) — midpoint outline circle.",
    "FCIRCLE": "FCIRCLE(X,Y,R,C) — current placeholder fills the bounding rectangle.",
    "TILE": "TILE(X,Y,CH,C) — write character/color cell in tile or ordinary text mode.",
    "CHARAT": "CHARAT(X,Y,CH,C) — alias for TILE.",
    "SPRSET": "SPRSET(N,ON,C,PATTERN) — enable/configure sprite and generate pattern; do this before SPRFILE.",
    "SPRMOVE": "SPRMOVE(N,X,Y) — update hardware sprite position.",
    "SPRROW": "SPRROW(N,ROW,B1,B2,B3) — write one 24-bit sprite row.",
    "SPRSIZE": "SPRSIZE(N,XON,YON) — set horizontal/vertical expansion.",
    "SPRPRI": "SPRPRI(N,BEHIND) — set sprite/background priority.",
    "SPRMUL": "SPRMUL(N,ON) — enable multicolor sprite mode.",
    "SPRCOL": "SPRCOL(N,C) — set sprite primary color.",
    "SPRMCO": "SPRMCO(C1,C2) — set shared multicolor sprite colors.",
    "SPRSCAN": "SPRSCAN() — latch sprite collision registers for polling.",
    "SPRCOLL": "SPRCOLL(N,C%) — read latched collision state.",
    "JOY": "JOY(PORT,J%) — poll a joystick port.",
    "KEYP": "KEYP(K%) — poll the current key.",
    "KEYSCAN": "KEYSCAN() — refresh keyboard polling state.",
    "KEYLAST": "KEYLAST(K%) — retrieve the last scanned key.",
    "POLY": "POLY(A%(0),COUNT,C) — outline integer-array coordinate pairs.",
    "FPOLY": "FPOLY(A%(0),COUNT,C) — conservative convex fan fill.",
    "PBMAKE": "PBMAKE(COUNT,H%) — allocate typed REU point buffer.",
    "PBUFSET": "PBUFSET(H%,INDEX,X,Y) — set a zero-based point.",
    "PBDROP": "PBDROP(H%) — release point buffer.",
    "POLYH": "POLYH(H%,COUNT,C) — outline points from REU buffer.",
    "FPOLYH": "FPOLYH(H%,COUNT,C) — convex fan fill from REU points.",
    "DLMAKE": "DLMAKE(COUNT,H%) — allocate one-page retained display-list handle.",
    "DLRST": "DLRST(H%) — clear retained records.",
    "DLPLOT": "DLPLOT(H%,X,Y,C) — append a plot record.",
    "DLLINE": "DLLINE(H%,X1,Y1,X2,Y2) — append a line record.",
    "DLRECT": "DLRECT(H%,X1,Y1,X2,Y2) — append rectangle outline.",
    "DLFBOX": "DLFBOX(H%,X1,Y1,X2,Y2) — append filled rectangle.",
    "DLDRAW": "DLDRAW(H%) — replay records in list order.",
    "CHRMAKE": "CHRMAKE(COUNT,H%) — allocate eight-page charset resource.",
    "CHRROW": "CHRROW(H%,CHAR,ROW,BYTE) — set one character bitmap row.",
    "CHRUSE": "CHRUSE(H%) — copy charset to visible Bank D layout.",
    "TSMAKE": "TSMAKE(COUNT,H%) — allocate one-page tileset.",
    "TSSET": "TSSET(H%,INDEX,CHAR,C) — define a tileset entry.",
    "TMMAKE": "TMMAKE(COUNT,H%) — allocate four-page 40x25 tilemap.",
    "TMSET": "TMSET(H%,INDEX,TILE,FLAGS) — set a map cell.",
    "TMDRAW": "TMDRAW(MAP%,TILES%) — render tilemap through tileset.",
    "MCELL": "MCELL(CX,CY,S1,S2,S3) — set multicolor bitmap cell palette attributes.",
    "MCBG": "MCBG(C) — set shared background color.",
    "SIDRST": "SIDRST() — clear SID registers $D400–$D418.",
    "SIDOFF": "SIDOFF() — alias for SIDRST; does not detach an IRQ player.",
    "VOL": "VOL(V) — master volume 0–15, preserving filter mode bits.",
    "FRQ": "FRQ(V,F) — raw 16-bit frequency, voice 1–3.",
    "PITCH": "PITCH(V,SEMITONE,OCTAVE) — PAL pitch table, semitone 0–11 / octave 0–7; does not gate voice.",
    "PULSE": "PULSE(V,WIDTH) — pulse width 0–4095.",
    "ADSR": "ADSR(V,A,D,S,R) — envelope nibbles 0–15.",
    "WAVE": "WAVE(V,MASK) — write complete voice control/wave/gate byte.",
    "GATE": "GATE(V,ON) — alter gate bit alone.",
    "VOICE": "VOICE(V,F,W,AD,SR) — packed frequency, control and envelope setup.",
    "FILTER": "FILTER(CUTOFF,RES,ROUTE,MODE) — cutoff 0–2047, resonance 0–15, route and mode masks.",
    "SOUND": "SOUND(V,F,D,W) — blocking tone; duration is spin-delay units, not milliseconds.",
    "MEMCAP": "MEMCAP(ADDRESS) — reserve memory by lowering the BASIC ceiling; see contract above.",
    "BORDER": "BORDER(C) — set low four border-color bits.",
    "USPEED": "USPEED(MHZ) — Ultimate-specific built-in speed control; see supported table above.",
    "UMHZ": "UMHZ() — Ultimate-specific live nominal speed query.",
}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    asm = (APP / "readybasic.s").read_text()
    block = asm.split("rb_command_descriptors:", 1)[1].split('.segment "HIDDEN"', 1)[0]
    commands = re.findall(r'^\s*CMD_[A-Z0-9_]+[^\n]*"([A-Z0-9]+)"', block, re.M)
    lines = [(ROOT / "docs/readybasic_reference_intro.md").read_text().rstrip(),
             "", "## Implemented built-in inventory", "",
             "| Command | Form and purpose |", "| --- | --- |"]
    for name in commands:
        description = CONTRACTS.get(name)
        if description is None:
            if name not in {"SLOT0", "SLOT1", "SLOT2", "SPAN", "OVL1", "OVL2"}:
                raise SystemExit(f"Document new command: {name}")
            description = f"{name}(R%) — developer proof of slot/span/overlay dispatch; returns a diagnostic integer."
        form, purpose = description.split(' — ', 1)
        lines.append(f"| `{name}` | `{form}` — {purpose} |")
    lines += ["", "## Developer sample-module commands", "",
              "The scalar, array, scratch-allocation and slot/overlay examples are disk-only.",
              "Their names have no Z prefix. PAUSE and LDMOD remain built in.",
              '`LDMOD("RBM.SAMPLE1",N%)` returns 12; SAMPLE2 returns 7 and SAMPLE3 returns 32.',
              "Load sample1 before running RBTEST1/RBPROC1 or typing the scalar examples.",
              "Those two programs also load it on line 5 when RUN.",
              "Sample1 and sample2 coexist. Sample3 replaces their demo registry entries",
              "and supplies its own COPY/CPYRST commands alongside 30 stateful entries.",
              "Reload sample1/sample2 when returning to those examples. All packages preserve",
              "the production commands and media registry; there are 49 distinct demo names",
              "and 51 package entries because COPY/CPYRST occur in two packages.", "",
              "Media occupies core-bank $1A40-$1B3F. Sample1 uses $1B40-$1CBF;",
              "sample2 uses $1CC0-$1D9F; sample3 uses $1B40-$1F3F.",
              "All disk payloads and the runtime must be rebuilt together.", "",
              "| Package | Command | Diagnostic behavior |", "| --- | --- | --- |"]
    values = {"SLOT0": 30, "SLOT1": 31, "SLOT2": 32, "SPAN": 40,
              "OVL1": 51, "OVL2": 52, "DM1": 61, "DM2S": 74, "DOV1": 72, "DOV2": 73}
    stateful = {str(c["name"]): c for c in rbm3_payload_commands()}
    for package, names in sample_inventory().items():
        for name in names:
            if name in stateful:
                c = stateful[name]
                purpose = (f"Stateful diagnostic, submodule {c['submodule_id']}, overlay {c['overlay_id']}; "
                           "paired A/B entrypoints share overlay-local state")
            elif name in values:
                purpose = f"Return {values[name]}; slot/span/overlay dispatch proof"
            else:
                purpose = CONTRACTS[name].split(" — ", 1)[1]
            lines.append(f"| {package} | `{name}` | {purpose} |")
    lines += ["", "Sample3 tests whether overlay-local state survives reuse and resets after",
              "another image replaces it. It is temporary state, rather than application storage.",
              "Workers live in `src/apps/readybasic/sample_low.s`; packaging and generated",
              "slot/overlay payloads are in `build_support/build_readybasic_disk_modules.py`.", "",
              "## Packaging by profile", "",
              "This inventory describes source configuration, not proof that every existing",
              "binary image has been rebuilt. Example counts exclude applications/utilities.", "",
              "| Profile | Example PRGs | Disk packages |", "| --- | ---: | --- |"]
    sources = sorted(APP.glob("*.bas"))
    by_stem = {p.stem: p for p in sources}
    availability = {p.stem: [] for p in sources}
    for pid in profiles.list_profile_ids():
        profile = profiles.load_profile(pid)
        contents = [c for d in profile['disks'] for c in d['contents']]
        examples = [c for c in contents if Path(c['artifact']).stem in by_stem]
        packages = sorted({c['name'] for c in contents if c['name'].startswith('rbm.')})
        if examples or packages:
            lines.append(f"| `{pid}` | {len(examples)} | {', '.join('`'+x+'`' for x in packages)} |")
        for c in examples:
            availability[Path(c['artifact']).stem].append((pid, c['name']))
    lines += ["", "EasyFlash preloads the runtime from CRT, but its companion data disk does",
              "not currently package this BASIC example/media collection. Supplying the",
              "runtime on cartridge is different from supplying the disk-loadable assets.", "",
              "## Every example at a glance", "",
              "| Example | Topic |", "| --- | --- |"]
    for p in sources:
        topic = example_topic(p.stem)
        lines.append(f"| [{p.stem}](#{p.stem.lower().replace('_','-')}) | {topic} |")
    names = set(commands) | {'MUSTUNE','MUSPLAY','MUSHALT','MUSDROP','RSCFILE','MCFILE','SPRFILE','MCLINE',
                            } | {n for names in sample_inventory().values() for n in names}
    for p in sources:
        source = p.read_text().rstrip()
        used = sorted(n for n in names if re.search(r'\b'+n+r'\s*\(', source, re.I))
        entries = availability[p.stem]
        disk_names = sorted(set(name.upper() for _, name in entries))
        lines += ["", f"## {p.stem} {{#{p.stem.lower().replace('_','-')}}}", "",
                  example_topic(p.stem), "",
                  f"[Original source](../src/apps/readybasic/{p.name}).",
                  "", "Commands used: " + ', '.join('`'+n+'`' for n in used) + ".", ""]
        if entries:
            lines += ["Disk name: " + ', '.join('`'+n+'`' for n in disk_names) + ".",
                      "Profiles: " + ', '.join('`'+pid+'`' for pid, _ in entries) + "."]
        else:
            lines += ["Historical source retained; no current profile disk entry. Use the newer combined demos."]
        lines += ["", "```basic", source, "```"]
    out = ROOT / 'docs/readybasic_reference.md'
    content = '\n'.join(lines)+'\n'
    if args.check:
        if not out.exists() or out.read_text() != content:
            raise SystemExit('ReadyBASIC reference is stale; regenerate it')
    else:
        out.write_text(content)
    print(f"{out}: {len(commands)} built-in descriptors; {len(sources)} complete BASIC sources")

if __name__ == '__main__':
    main()
