# ReadyBASIC suites on the C64 Ultimate

`../run_readybasic_c64u_suites.py` reuses the existing generated ReadyBASIC
plans and runs their assertions through `vice_tasks_dotnet run-ultimate-plan`.
It does not start or stop VICE. Run it from a Terminal-owned background shell,
as required by the repository's `AGENTS.md`.

## Prepare the disk

Build through `run.sh --profile precog-ultimate --config <test-catalog.ini>
--run-first readybasic --build-only`. Choose a fresh path under
`/USB1/automation/readybasic-suites/<unique-folder>/<image>.D81` and set that
exact path in the catalog's `c64u_image_path`. Keep `dma_loading=1`.
The runner extracts the finished disk's `apps.cfg` and checks the DMA flag,
startup app, and path before uploading anything. It uploads only into a new
folder, verifies the complete image by FTP readback, and remounts it unlinked
for each cold suite. It never overwrites a pre-existing test folder. Recovery
may use `--reuse-verified-image` to read back that exact image without writing
it again. Each suite clears the REU before booting so reset cannot resume an
older cached app.

## Run

Install PyYAML in a temporary Python environment and build this video helper:

```sh
dotnet build build_support/readybasic_c64u_video/Video.csproj
python build_support/run_readybasic_c64u_suites.py \
  --disk /absolute/path/to/test-boot.d81 \
  --examples-disk /absolute/path/to/test-examples.d81 \
  --remote-image /USB1/automation/readybasic-suites/unique/TEST.D81 \
  --video-helper build_support/readybasic_c64u_video/bin/Debug/net8.0/Video.dll \
  --out build/readybasic-c64u-suites/unique-run \
  build_support/readybasic_module_overlay_probe.generated.yaml
```

For the Ultimate D81 pair, `--disk` is the drive-8 boot image and
`--examples-disk` is the drive-9 demo image. The adapter uploads the companion
as `EXAMPLES.D81` in the same fresh folder, verifies both images, mounts both
drives, and routes only named demo LOAD commands to device 9. LDMOD/media
loads stay on device 8. Omit `--examples-disk` for older single-image fixtures.

The sibling `agenticdevharness` checkout supplies the .NET source. The adapter
builds an isolated copy inside the run directory, enables its existing
`ultimate.clear_reu` operation in the validator where needed, and uses its
existing ten-byte keyboard-buffer input helper. Shared runner binaries are
not replaced. Source hashes are retained beside the isolated build.
`VICE_TASKS_BINARY` may instead select a previously prepared runner build.
Multiple generated plans may follow the options. Each suite cold boots normal
ReadyOS through PREBOOT and the launcher.

## Video gates

C64U memory reads during loading can stall disk transfers. Even the harness's
keyboard helper polls RAM after injecting RETURN. The adapter therefore types
the command text while idle, injects its final RETURN without polling, and
waits at a video-only gate. This helper captures UDP video frames only; it does
not read screen RAM or machine state.

Inspect the PNG named in the run directory's `pending.json`. After visibly
confirming the relevant load has finished, create the `acknowledge` file named
there, with a short record of what was observed. Never acknowledge based only
on elapsed time. The run then resumes the original harness assertions.

Results and original harness manifest paths are recorded in `results.json`.
A `complete` file is written only after every selected suite passes.

For a test-only correction on an unchanged, idle machine, `--resume-from ID`
runs the remaining range of one plan without rebooting. It verifies the mounted
image and waits for video confirmation first. `--resume-command` records an
explicit BASIC fixture repair if needed. A resumed run records `tail_passed`;
combine its assertions with the verified prefix before claiming full coverage.

`confirm_ready.py <run-directory> <patterns.json>` optionally confirms idle
prompts from video pixels. It requires Pillow and two 48-by-8 binary glyph
masks (`upper` and `lower`, eight strings of `#`/`.`), taken from visually
verified READY. prompts. `ready-patterns.json` contains the masks observed on
the physical PAL C64 Ultimate. It matches the final text row in two distinct PAL
frames, rejecting any following text except the cursor. Launcher screens and
unrecognized output remain pending for inspection. It reads local PNGs only.

CPU-register diagnostic steps are explicitly recorded as omitted because
Ultimate REST does not expose the VICE CPU monitor. Keyboard-buffer monitor
commands are translated to equivalent Ultimate input. Unknown monitor commands
fail rather than silently passing; the VICE-specific raw execution probes need
their own hardware adaptation before using this runner.
