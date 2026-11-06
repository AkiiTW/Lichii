# Ocarina of Time PGO profiles

Raw profiles (`.profraw`, or `.zip` archives of them) recorded with the `pgogen` flavor of
`lichii-tuned` (only `video_core` and `shader_recompiler` instrumented). The workflow merges them
into the profile the `normal` flavor uses, each weighted 4 over `eglantine.profdata`.

- `ocarina-village-cd1f029.profraw`: build cd1f029, ~6.5 min in Kokiri Forest (usual spot, wide
  view, camera turns in the village), RTX 5060 Ti, Ryzen 7 3800X.

A profile only fits the code it was recorded with: record a new one after large changes to the
GPU emulation (stale functions are skipped with -Wno-profile-instr-out-of-date).
