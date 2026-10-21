# Frame-independent simulation policy

The user explicitly authorized correcting original frame-rate dependence on
2026-10-22. This modern scheduling policy is not a reverse-engineering claim.
The original physics frequency remains unconfirmed. No newly designed clock or
interpolation helper receives an RE marker or advances reverse-engineering status.

## Simulation and presentation

`game_src/core/simulation_clock.hpp` owns the accumulator. Live free-driving
constructs GameEntry with `fixed_simulation`; its recovered stages receive exactly
float1/60seconds per tick, independently of display FPS.60Hz preserves the existing
validated reference driving feel. Changing this policy frequency is a gameplay
change because the recovered legacy solver still contains per-callback operations;
it must not be inferred from rendering FPS or modified as a quality setting.

The original variable-step/.1-second clamp remains available only through the
explicit `legacy_reference` policy for strict scaffolds, diagnostics and original
scheduler tests. The live fixed clock receives elapsed time before that legacy
clamp, so a long frame is subdivided rather than shortened. Up to240 ticks are
processed per callback; excess time remains owed for subsequent callbacks. There
is no discarded remainder, enlarged physics dt or artificial slowdown multiplier.

Input is sampled by the host; continuous controls are held for each simulation
step until the next input sample. Reset is a rising-edge command: a short press
is latched across a frame with no tick, and held reset cannot execute repeatedly
within catch-up. Quit is handled immediately, including frames with no tick.
Input latency still depends on when the platform delivers events. Equality tests
require equivalent input histories on the simulation timeline; different inputs
are not claimed equivalent merely because total run duration matches.

Vehicle forces, contacts, transmission, wheels, effect triggers, CPU particle pools,
skid sampling, chase camera and audio command cadence all advance on this same
fixed simulation timeline. Audio mixing/playback remains Godot-owned real-time
work. No Godot vehicle physics substitutes for the recovered solver.

RenderingServer stores previous/current simulation transforms and uses Godot's
Transform3D interpolation for body/wheels/world instances and the camera. The
accumulator fraction interpolates visuals only, with one fixed-step presentation
latency. Static transforms are skipped. Spawn/reset snap both snapshots to prevent
teleport smearing. Interpolated camera position drives view-distance/fog/sky-height
settings and particle billboards; it never updates simulation positions, camera
controller state, contact caches or audio-source logic.

## Godot host time

Godot's own main-loop source can shorten process_step when its physics catch-up
limit is exceeded. Merely subdividing the supplied `_process(delta)` would still
lose wall time after stalls. The adapter therefore measures elapsed microseconds
through Time.get_ticks_usec and applies Engine.time_scale before accumulating.
Node pause/unpause resets the wall-time anchor so an intentional scene pause does
not become catch-up debt. Time scale0 freezes simulation while presentation/UI
can continue. Loading/restarting resets the anchor after initialization.

Primary implementation reference:
https://github.com/godotengine/godot/blob/4.7-stable/main/main.cpp
(Main::iteration, max_physics_steps/process_step adjustment).

## Required validation

```powershell
python scripts/smoke_race.py --frame-independent
python scripts/smoke_race.py --timing
ctest --preset core --output-on-failure
```

The portable original-asset probe compares every recorded physics step across
10/15/30/60/75/120/144/240FPS and irregular350/900ms stalls for20seconds/1200ticks.
It compares float bit patterns for vehicle pose/velocity/rates/RPM/controls,
suspension/airtime, gear/wall-contact counts, effect sources and collision-audio
events. The baseline includes acceleration, steering, braking/reverse, reset and
actual walls/landing. It does not require Godot or mutate original assets.

The native timing smoke exercises GameEntry through the real host, compares
vehicle/effect state across render-rate schedules, presents interpolated frames,
checks RID teardown, and separately tests a real350ms Godot main-thread stall and
Engine.time_scale0. Standalone clock tests cover zero-tick input edges, held reset,
ten-second retained catch-up debt, invalid time and callback failure. These are
modernization regressions, not original-executable behavioral equivalence.
