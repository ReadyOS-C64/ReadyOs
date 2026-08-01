# C64U DMA Acceptance Configs

These ReadyOS catalog fixtures exercise the launcher DMA path cases that matter
on C64 Ultimate hardware:

- `precog-d81-dma-valid.ini`: DMA enabled with a configured image path. Upload
  or copy the tested D81 to the same C64U path before running.
- `precog-d81-dma-empty-path.ini`: no configured C64U image path, so the
  launcher must skip DMA setup and use normal disk loading.
- `precog-d81-dma-bad-path.ini`: configured path cannot be mounted, so the
  launcher must report DMA unavailable and fall back to disk loading.

The valid-path fixture intentionally uses `/usb1/readyos.d81` as a stable test
path. Versioned deployment builds may override this value, but the value inside
`apps.cfg` must exactly match the file path present on the C64U USB volume.
