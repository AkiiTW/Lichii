# Ocarina of Time PGO profiles

Raw profiles (`.profraw`, or `.zip` archives of them) recorded with the `pgogen` flavor of
`lichii-tuned` (only `video_core` and `shader_recompiler` instrumented). The workflow merges them
into the profile the `normal` flavor uses, each weighted 4 over `eglantine.profdata`.

- `ocarina-market-field-kokiri-ca4f7d0.profraw`: build ca4f7d0, ~3.5 min each in the Castle Town
  market (fountain laps, camera moving), Hyrule Field (running, looking around), then on foot to
  Kokiri Forest and ~3.5 min in the village; switches clean_words, BANK_COMPARE, TC_RT_NO_UPLOAD=3,
  TC_RT_EVICT=4, no measuring; RTX 5060 Ti, Ryzen 7 3800X. Replaces `ocarina-village-cd1f029`
  (Kokiri only, recorded before the texture cache and buffer work).

A profile only fits the code it was recorded with: record a new one after large changes to the
GPU emulation (stale functions are skipped with -Wno-profile-instr-out-of-date).
