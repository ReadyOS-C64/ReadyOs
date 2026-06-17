# Lessons

- At 1MHz, BASIC-side resource construction loops are slow enough that screenshot automation must wait for an explicit program-held graphics frame, not an optimistic fixed delay.
- Command names containing tokenizable BASIC words are risky without changing the language tokenizer. `DLMAKE`, `CHRMAKE`, `TSMAKE`, and `TMMAKE` are safer than `*NEW`.
- Even existing names can be unsafe in stored BASIC if they contain tokenizable substrings; `GFXTGT` is the safe demo spelling for `GFXTARGET`.
- Shared scratch buffers overlap intentionally. Before fetching larger REU chunks, check whether any saved state lives inside the destination range.
- For speed, retained resources should be compact and REU-backed. BASIC should build handles or load future resource files; command overlays should perform the expensive traversal/copy work.
- The tilemap command path and the tile/charset display path are separate failure domains. Debugging them separately avoids blaming the REU structure when the visible VIC configuration is the real issue.
