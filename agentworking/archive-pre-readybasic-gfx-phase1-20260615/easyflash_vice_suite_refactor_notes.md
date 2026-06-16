# EasyFlash VICE Suite Notes

## Goal

Keep cartridge automation mechanically equivalent to the regular ReadyBASIC and
ReadyShell VICE automation after the REU/app/module allocation refactor.

## Approach

- Generate the regular VICE YAML plans from the existing probe scripts.
- Transform only the `global_defaults.vice` block to boot the EasyFlash CRT and
  attached data D64.
- Insert one cartridge-only launcher selection step after `vice.launch`.
- Preserve all downstream input sequences, assertions, dumps, and cross-app
  launcher navigation from the regular plan.

## Launcher Start Points

- `editor`: current EasyFlash launcher selection, so press Return.
- `readyshell`: cursor down once, then Return.
- `readybasic`: cursor down four times, then Return.

## Coverage

ReadyBASIC cartridge plans mirror:

- demo suite
- repeat label probe
- lifecycle probe
- module overlay probe
- plugin command probe
- program probe
- rbtest1 probe
- resume-min probe
- screen/REU temporary buffer probe
- state probe
- large-vars probe
- cross-app resume probe
- second-entry editor probe
- full visual verification suite

ReadyShell cartridge plans mirror:

- cross-app resume probe, starting from Editor through the EasyFlash launcher

## Temporary Disk Handling

The runner copies `readyos_data.d64` to
`agentworking/easyflash_full_vice_plans/readyos_data.easyflash-vice.d64`.
ReadyBASIC test-only fixtures are written only to that copied D64:

- `rbtest1`
- `rbproc1`
- `rbprocerr`
- `rbscrreu`
- `rbm.sample1`
- `rbm.sample2`
- `rbm.sample3`

## VICE Cold-Start Handling

One repeated cartridge launch reached the EasyFlash booter and timed out on
`READY OS`; rerunning the same generated plan immediately passed. The runner
therefore gives each cartridge plan a short cold-start pause and one whole-plan
retry. Assertions inside the mirrored plans are unchanged.

## Probe Robustness Fix

`run_readybasic_resume_min_probe.sh` now clears the screen before printing its
initial `STATE` marker. The cartridge ReadyBASIC screen keeps launcher/status
text near the bottom; without the clear, the state value printed over a status
line and made the assertion depend on stale UI characters instead of BASIC
state.

`run_readybasic_screen_reu_temp_probe.sh` now waits up to 300s for the
`RBSCRREU DONE` marker instead of doing an immediate assert after a fixed delay.
The cartridge path can take longer to finish the screen/REU pause-and-restore
loop; a failure dump showed the marker present after the shorter timeout.
