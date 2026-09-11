# Ultimate recovery helpers

These previously local helpers are preserved separately from ReadyBASIC work.
They are manual diagnostics, not part of a build or an automatic test suite.

- `capture_xuzio_live.sh` and `capture_xuzdeflate_live_c64u.sh` capture live
  memory for an already-running probe. They do not start or validate that probe.
- `list_xuzio_ftp_entry.sh` and `fetch_xuz*_partial_c64u.sh` retrieve listings
  or partial outputs; successful downloads alone are not a codec pass.
- `resume_xuzextract_downloads_c64u.sh` and
  `recover_uzip_workflow_oracle_c64u.sh` recover host-side verification for an
  existing owned run. They require its local fixtures and remote owner marker.
- `fix_xuzstore_sentinel.sh` is **not read-only**: after checking an existing
  stage-9 pass result, it rewrites the screen sentinel. It does not rerun the
  test and must not be used as independent evidence of a passing test.
- The corresponding `start_*_terminal.sh` wrappers run through Terminal as
  required for reliable C64 Ultimate connectivity from Codex. Other helpers
  must likewise be launched from a Terminal-owned/background shell.

Review paths, host, current machine state and output destinations before use.
Several defaults are specific to the original workstation. Network scripts
were not executed during the commit cleanup; validation was limited to shell
and embedded Python syntax. Generated captures and fixtures remain uncommitted.
