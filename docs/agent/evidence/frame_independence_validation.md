# Frame-independence acceptance

2026-10-22, Windows x64, GNU13.2 C++20, Godot4.7.2-stable, Compatibility,
Intel Arc B390. This validates the user-authorized modernization described in
../frame_independence.md, not original-executable equivalence.

Passed independent core configuration/build and28/28 core CTests. The new clock
test covers fixed step size, reset edges across zero-tick frames, held reset,
ten-second catch-up debt without discarded time, invalid inputs, callback failure
and the GameEntry fixed-policy path. Existing legacy scheduling tests still pass.
Boundary, generated-class and inventory/progress checks pass.

Original Dark Forest/Animal resource probe:20seconds/1200simulation ticks at
10/15/30/60/75/120/144/240render FPS all have bit-identical recorded per-tick traces.
The same holds for irregular schedules containing350ms and900ms stalls. Compared
state covers pose, velocity, angular rates, RPM, gear, controls, airtime/suspension,
wall-contact counters, tire/skid source positions/gates and collision-audio events.
Inputs are replayed on the same simulation timeline; they are not frame-indexed.

Native GameHost timing smoke passes at all eight render-rate schedules, with
identical300-tick vehicle/effect state after5seconds. It explicitly exercises
the render interpolation path and verifies no owned RIDs remain after each scene.
A separate actual350ms main-thread block recovers23simulation ticks including
the following normal callbacks; Engine.time_scale0 freezes the simulation.
The native timing log is retained in frame_independence_validation.json.

Native regressions also passed: chase/manual camera state transitions, driving
and skid pixels, tire/Crash/Land mixer/loop/one-shot behavior, particle visibility
and restart lifecycle, diagnostic triangle10restarts and teardown. Particle
visibility comparison counted34,084 changed pixels; driving counted1,510 skid
pixels. These are image/integration checks, not proof of unimplemented gameplay.

The extension was built and loaded by the native acceptance commands. An isolated
build/frame_physics configuration avoids shared Ninja-cache contention from
parallel project work. The final incremental build compiled its changed objects,
but could not replace the DLL because another running Godot instance held it open.
The latest isolated CTest run passed 29/29 tests, including the concurrently added
debug-overlay check; the JSON records the earlier 28-test acceptance snapshot.
Source hashes, native binary hash and compact acceptance records in that snapshot
identify the tested revision, rather than subsequent concurrent edits.
No original binary, resource, executable or DLL was executed or modified.
