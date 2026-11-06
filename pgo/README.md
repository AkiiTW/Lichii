# Ocarina of Time PGO profile

1. Run the `pgogen` artifact of the lichii-tuned workflow (an instrumented, slower build).
2. Play Ocarina of Time for 5-10 minutes in the places that matter (Kokiri Forest, the village
   wide view, combat, menus), then close Lichii normally (File > Exit).
3. `default_*.profraw` files appear next to lichii.exe. Zip them and upload the zip here (pgo/).
4. The push rebuilds the `normal` and `lto` flavors with this profile merged in.
