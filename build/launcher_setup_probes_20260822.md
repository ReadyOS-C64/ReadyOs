# Historical launcher / SETUP probes — 2026-08-22

The six dated YAML plans and seven `start_*_20260822.sh` launchers in this
directory preserve the original hand-authored reproduction steps. They cover
missing-path guidance, valid DMA paths, continuation/screen captures, and the
standalone SETUP browser's cancel path.

These are historical diagnostics, **not current, portable regression tests**.
They contain absolute workstation paths, old image names, assumed mounted
images and keyboard navigation tied to the directory state at that time.
The referenced generated D81 fixtures are intentionally not committed here.

Do not run them blindly: they can reset the machine, mount images, change CPU
speed and send keystrokes. Any future reuse must follow the current AGENTS.md
boot, Terminal-owned network and uniquely owned test-storage rules. The older
direct BOOT sequences are not the current ReadyOS launch cookbook.

This archival commit preserves the files without executing hardware actions or
claiming fresh hardware results. Generated fixtures, compiler output, logs and
release-image churn are excluded.
