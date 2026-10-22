# Native frame pacing attribution and loading preparation

This is a modern adapter performance investigation, not an original-executable
behavior or reverse-engineering completion claim. The portable simulation policy
and recovered per-step formulas are unchanged.

## Reproduction and measurement

The configured Windows host used Godot 4.7.2, Vulkan Forward+, a 1280x720 window,
120.00017 Hz display, VSync Enabled, Dark Forest, Animal, five AI, three laps,
seed 1 and the same recovered autopilot for the player. No driver cache was
cleared and no VSync setting or GPU driver option was changed.

`python scripts/profile_frames.py --seconds 65 --label capture` runs the native
capture. `--without-debug` provides the overlay-off control. The host-only
`--frame-trace` enables a preallocated, bounded 8192-record trace. There is no
per-frame file logging. The capture exports JSON and flat CSV into ignored build
artifacts only after disabling live processing. It includes host entry timestamps,
callback intervals, stage durations, simulation tick numbers, resource creation
counts, file-read attempts, pipeline counters and the last completed viewport's
CPU/GPU render durations. Those GPU measurements are not scanout timestamps.

A row's interval runs from the preceding host entry to this entry: attribute its
CPU work to the **preceding** row. Pipeline counters and viewport measurements
are returned by the renderer and can lag submissions; inspect adjacent rows.
Probe overhead was separately recorded (baseline maximum 118 us). Initialization
and the first two seconds are excluded from the steady-state comparisons below;
startup pipeline work is not hidden from the raw trace.

The live graph now samples at host callback entry, before debug snapshot generation.
Sampling within overlay drawing could otherwise show an artificial long/short
pair when diagnostic work before the sample point changed. Manual test draws do
not add samples. The graph still measures CPU frame intervals, not GPU work.

## Findings

The 25-second baseline had 2,761 steady-state intervals, with 26 intervals over
12.5 ms and a maximum of 21.284 ms. Every one of those 26 intervals followed
creation of a new native particle system. Combat visual submission reached
13.715 ms, versus a normal sub-millisecond path; each creation also coincided
with a new Surface pipeline. `create_particles` created fresh process and draw
Shader RIDs and set their identical source code for each emitter. Thus CPU-side
resource/shader preparation was repeated for equal particle layouts.

The overlay-off control still reproduced three long intervals, all following
particle creation, with a 13.735 ms maximum combat-visual stage. The debug
snapshot compounded the problem: even with route drawing disabled, every frame
reprojected all route nodes onto the ground, taking up to 4.803 ms. The periodic
display query reached only 37 us in the baseline; audio creation reached 64 us,
and GPU render time peaked at 3.527 ms. These were not the captured primary cause.

A 65-second intermediate capture exposed additional first-use work after combat
started: ammunition for Grenades, Land Mines and Reactive Mines, cannon effects
and missile flares still loaded textures/materials and uploaded meshes on first
visibility. Combat visual submission reached 10-27 ms in those frames. The
longer capture was necessary; a clean initial driving segment was insufficient.

## Changes

Particle process shaders are cached by capacity; draw shaders by capacity/blend
variant. Emitter process/draw materials remain separate so positions, colors,
atlas choices and custom triangles cannot overwrite another emitter's uniforms.
Shared shaders are renderer-owned, released after all particle owners, and
included in RID accounting. Native regression checks shader sharing, per-emitter
uniform isolation and survival of one emitter after another is destroyed. The
dummy headless backend does not support the material readback portion.

Loading prepares hidden particle templates using the session's actual effects
scalar, bounded existing pools and selected weapon impact/trail configurations.
Known inactive tire/spark/nitrous pools are instantiated by the initial submission.
Selected weapons' body, muzzle, housing, ammunition, flare and leg meshes are
prepared using their actual fixed projectile capacities. This only creates
presentation resources: it does not fire a weapon, draw random numbers, register
callbacks or advance a physics/effect tick. Unselected weapon assets are not
loaded globally. Some dynamic impact emitters and changing geometry still create
instances during play, but reuse prepared shader variants and materials.

`debug_snapshot(false)` omits route ground projection while retaining participant,
equipment and collision diagnostics. The host requests route geometry only when
route/checkpoint display is enabled. Default full snapshots remain available to
existing callers and tests. Read-only snapshot regression compares gameplay state.

A native timing fixture initially failed because its new particle test submitted
one slot to a 15-slot system, then indexed an unavailable dummy-backend readback.
The fixture now supplies the full pool and limits readback checks to the native
renderer. Headless shutdown also now waits 100 ms for the audio mixing thread to
release AudioStreamWAV/AudioStreamPlaybackWAV references; verbose output identified
those references rather than a renderer RID leak.

## Final captured result

The final isolated 65-second capture (`frame_profile_final.json/.csv`) contained
7,562 steady-state intervals after the two-second exclusion:

| Metric | Final capture |
| --- | ---: |
| Intervals over 12.5 ms | 0 |
| P99 callback interval | 8.720 ms |
| Maximum callback interval | 10.669 ms |
| Maximum debug snapshot | 0.236 ms |
| Maximum combat visual submission | 2.443 ms |
| Maximum host callback work | 6.573 ms |
| Maximum viewport GPU render time | 3.014 ms |
| Runtime texture/material creation | 0 / 0 |
| Runtime installation-file read attempts | 0 |

The final 600-sample graph window averaged 8.33135 ms, P99 8.680 ms and maximum
9.020 ms. Dynamic work remained observable: 106 particle-system creations,
511 mesh uploads and 155 audio voices across the steady-state trace. They no
longer caused the measured threshold violations. This is evidence for this
hardware/profile, not a guarantee about every track, loadout or external OS/GPU
scheduling event. Logs and raw traces remain ignored build artifacts.

## Validation

Both build presets passed. All 66 core CTest cases and the two native adapter
CTest cases passed. The original-asset portable race reached an actual one-lap
finish and exercised the compact/full snapshot state check. Native Vulkan race
acceptance passed countdown, Inspect switching, pause/restart, moving-pause pixel
checks, actual finish/results, return to menus and repeated RID teardown. Native
and headless timing smoke passed, including shared-particle uniform ownership on
the native renderer. Progress and portable boundary checks passed. The final
65-second timing capture ran without concurrent builds or other render tests.

Godot's primary explanation of first-use pipeline work and hidden loading-time
instances is available at:
https://docs.godotengine.org/en/stable/tutorials/performance/pipeline_compilations.html
