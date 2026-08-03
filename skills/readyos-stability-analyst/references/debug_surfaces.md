# Debug Surfaces

Primary runtime capture source:
- `logs/vice_auto_*/manifest.json`
- `logs/vice_auto_*/trace.md`
- `logs/vice_auto_*/state/*.json`

## RAM debug surfaces
- Screen buffer: `$0400-$07E7`
- The former boot preload markers at `$C007-$C00C` are retired; preload no
  longer writes diagnostics into app snapshot RAM. Use launcher state, the
  ReadyOS-bank audit page, and harness stage traces instead.
- Shim data: `$C820-$C83F`
- Historical shim debug ring storage: `$C980-$C99F`; overlaps current
  `$C960-$C99F` logical REU setup code and must not be treated as active
  persistent debug storage.
- The former ReadyShell RAM ring at `$C7A0-$C7DF` / `$C7F0` is retired.
  ReadyShell diagnostics are authoritative only in its loader-assigned REU
  state bank.
- REU registers: `$DF00-$DF08`
- ReadyShell overlay window base:
- release/default (`READYSHELL_PARSE_TRACE_DEBUG=0`): `$8E00`
- debug trace (`READYSHELL_PARSE_TRACE_DEBUG=1`): `$8B00`

## REU debug surfaces
- ReadyShell state-bank-relative diagnostic head: `state_bank:$7DE0`
- ReadyShell state-bank-relative diagnostic payload:
  `state_bank:$7DF0-$7FEF`
- ReadyShell state-bank-relative probe byte: `state_bank:$7FFF`
- ReadyShell overlay cache previews must be resolved from logical REU bank `0`
  rich resource records or the generated ReadyShell overlay metadata. Do not
  assume fixed physical banks `0x40`, `0x41`, `0x43`, or `0x48`.
- Logical REU bank `0` contains the ReadyOS control-bank mirror, app/resource
  ownership records, and the shim token-to-physical-bank lookup page.

## Existing probes/tools
- `tools/vice_readyshell_automation.py` (captures manifest/trace/state)
- `tools/readyshell_reu_probe.py` (overlay compare + REU ring dump)
- `tools/vice_tasks/*` (composable task runner primitives)

Use artifact-first analysis; use live capture only when artifact evidence is missing or contradictory.
