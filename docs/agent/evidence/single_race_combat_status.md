# Single Race combat reconstruction coverage

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
All original-address claims refer to this binary. No original executable was run.
Portable classes under unresolved are explicit new owners of recovered state;
their English names do not establish original C++ class names or source paths.

## Live race integration

RaceSession loads the original equipment ordering and track-selected BEST tires,
then applies actual tire geometry/friction/scale and drivetrain bonuses to every
vehicle. VehicleDynamics owns internal/armor HP, nitrous state and upgrades.
Engine torque and AI damage-dependent mode selection read actual internal HP.
RacePresentation receives the same tire selection for player and opponent meshes.
Debug equipment views are read-only and identify actual weapon instance state,
ammunition, readiness, pending deployment and target separately from selection.
Selected paint textures are applied to player and opponent body meshes.
The launcher exposes --seed as a diagnostic CRT seed; default1 is not a recovered
original startup seed. AI counts below5 are a diagnostic extension of the original
six-slot race. Track/car/lap/AI options remain available.

Live player/AI weapon dispatch and scene turret combat are connected, including
actual projectile/effect pools, damage, armor and impulses. The earlier blanket
weapon-input failure has been removed. This does **not** establish whole-system
completion: original-executable traces, pixel equivalence and native spatial
audio remain unverified. The shared global callback registration/search/cursor
behavior is now recovered and connected; its unusual tail/cursor behavior is
preserved rather than replaced by an ordinary callback list. Missing required services return explicit
errors; they must not silently fall back to an unarmed race.

Portable presentation consumes actual meshes/poses, trail/impact/rear blast pools,
flame particles, oil triangles, skeletal Electron geometry, muzzle flashes,
missile flares, articulated Arachnoid legs and vehicle damage mesh/paint state.
Nonfactory scene Electron traps, snow and per-Stack chimney pools are connected. Equipment HUD uses
the original icon panel, ammo/lock tint and car-specific damage crops. Original
undefined initial smoke UV is explicitly reported instead of assigned a guessed
atlas frame. Native pixel/audio equivalence remains unverified.

## Recovered combat components

| Component | Implemented behavior | Integration/verification boundary |
| --- | --- | --- |
| Equipment | Slot availability/order, selection draws, armor/drive levels, track tires | Synchronous Reactive Mines constructors and reset/particle draws share the recovered CRT stream; native trace comparison remains |
| AI/player commands | Original lock gate, mount count, sector/route decisions, guided target selection, key configuration and input edges | Fresh Single Race uses confirmed20-second lock; mutable-cursor global callbacks are connected |
| Five gun/cannon families | Original pools, muzzle order, salvo cadence, motion, collision priority, damage routing and Vulcan impulse | Actual meshes, particles and race dispatch are connected; audio/pixel verification remains |
| Rocket Launcher | Distinct uncapped acceleration, geometry-based wall/car/ground hits, direct damage and impulse | Actual smoke pool supplies lifecycle/readiness; no invented radial damage |
| Guided/Back/Swarmers/Excalibur | Subtype launch, homing, jam decay, route following, RNG draws, expiry/readiness | Collision/explosion and real smoke pools are connected through race-owned services |
| Electron Cannon | First participant/edge beam hit, four-side damage order, sound/lifetime gates | Skeletal beam and impact pools are connected; native equivalence remains unverified |
| Flame Thrower | Original 100-particle lifecycle/RNG/order, midpoint damage and velocity damping | Lower-atlas additive display and live damage are connected; native equivalence remains unverified |
| Jammer | Original radius, timing, visual RNG and whole-weapon jam commands | Actual guided views, whole-weapon jam and native mesh/color are connected |
| Oil Slick | Four-use emission, projected collision/render triangles, contact counting, grip override | Wheel grip and actual render triangles/atlas draws are connected |
| Smoke Screen | Emission/expiry gate | Actual asymmetric smoke pool, render lifecycle and recovered audio cues are connected |
| Grenades/mines/Arachnoids | Distinct deployment owners and explicit instance-launch service contracts | Distinct instance kernels and specialized blast pools are connected; no generic fallback |
| Spikes/Ram | Front armor reductions are in vehicle equipment effects | Distinct contact animation and copied damage-force augmentation are connected |
| Scene turrets | Actual scene discovery, weapon kind, character exemption, 3D range, scan interval, prediction, arc/pitch limits, firing arguments | Actual assets, poses, shots, damage, trails and shared global callbacks are connected |

Detailed address tables, numerical observations and gaps are in race_loadout.md,
race_ai_combat.md, vehicle_equipment_effects.md, weapon_runtime.md,
weapon_projectiles.md, guided_weapon_runtime.md, special_weapon_effects.md and
rear_weapon_effects.md. The factory table in weapon_runtime.md covers all21 names;
factory coverage alone is not whole-system completion.

## Validation meaning

Unit tests exercise recovered contracts and explicit unavailable cases.
combat_frame_test connects turret, salvo, projectile and damage kernels in an
explicit synthetic fixture and additionally checks guided/jammer/flame state,
shared RNG and particle ordering across eight render rates and irregular stalls.
Synthetic trail counts and muzzle fixtures are test inputs, never live fallbacks.
The original canonical +Z turret desired-angle360 quirk is preserved by a separate
regression rather than silently normalized away.

Original-asset tests load all21 weapon parameter profiles and every installed
track's turret geometry, plus Dark Forest and RWA Speedway race equipment.
These checks do not establish original-executable or pixel equivalence.

## Current integration checks and explicit limits

The fresh Single Race caller flag and constructor stack copy now establish the
20-second weapon lock (single_race_weapon_lock_ghidra.json). Diagnostic seed and
reduced AI counts remain explicit extensions. Slot input uses original press
edges; SimulationClock retains a press/release occurring during zero-step frames.
Vehicle/body attachment transforms use native summed Euler ancestry; serialized
dummy basis is retained as metadata, not invented runtime rotation. Finish/death
continues world simulation and uses the actual track camera and keyboard-reset
10-second idle menu timer. Terminal HUD forces the map. The independent original
WEAPONS LOCKED overlay follows its own countdown/time/death gates, native300,5
anchor, red/yellow colors and pre-HUD ordering (weapon_lock_warning.md).

Body damage includes original100 texture-mark/four-deformation caps, eight random
draws per mark and canonical-path shared paint surfaces. All12 installed cars and
six paints load through the damage owner. Source class names remain unresolved.
The per-item normal/restriction/end/reload ledger is vehicle_combat.md; native
trace comparison and integrated original-behavior verification remain outstanding.
Undefined native particle atlas state is explicitly omitted/reported, and initial
weather camera history uses a labeled diagnostic seed. Audio emits original PCM,
repeat counts and loop events; QMDX spatial attenuation, Doppler and channel
allocation are not restored. These limits prevent claiming full native parity.

## Portable validation after warning integration

The core CTest profile passed66/66. Original-asset race_frontend_test validates
warning placement, gradient, hidden-HUD/death/finished conditions and restored
ordinary text colors. race_session_combat_lifecycle_test reaches all21 factory
names with diagnostic seeds, checks the original20-second lock and first-zero
countdown delay, retained zero-step input, pause, restart and live-effect teardown.

The full original-asset race_session_test frame-independence profile passed at
10,15,30,60,75,120,144,240 FPS with7157 bit-identical complete trace states per
cadence, followed by350/900ms stalls without discarded elapsed time. This compares
portable equipment, damage, callbacks, particles and shared RNG across schedules;
it does not compare those states against an original-executable trace.

## Diagnostic track switching and build validation

The new Race Debug Track dropdown reads available entries from the installed
SelectTrack configuration. Its request is consumed after UI drawing; a different
track tears down and reloads the race while retaining car/laps/AI/diagnostic seed,
debug preferences and pause/menu state. This is new diagnostic infrastructure,
not a claim that the original game provided in-race track switching.

Headless and native Vulkan competition smoke tests passed after dependency-cache
repair, with the existing intro-camera assertion unchanged. The new
race_track_switch_smoke_test covers an actual dropdown click, four reloads,
canonical-current-track no-op, invalid selection rejection, resource counts,
pause/menu/preferences restoration and actual simulation after resuming.

The build entry defaults to incremental build only; --run explicitly launches
and --clean requests native clean-first. Ten CLI tests, native header-change and
clean-first fixtures, cross-process build locking and repeated core/Godot no-op
builds passed. Six pre-existing empty Ninja dependency records were repaired;
their original corruption cause was not established. Rebuilding those objects
removed the stale RaceSession layout mismatch observed by the competition smoke.

## CLI Single Race launch randomization (2026-10-22)

The --race launch continues to use RaceSession::load and RaceLoadoutTable::assign
for every participant, including the player: paint, available weapons and equipment
follow the existing recovered ordering. The launcher no longer injects seed 1 when
--seed is omitted. GameHost selects a fresh time-derived seed once at startup; an
explicit --seed still overrides it for reproducible diagnostics. This seed policy
is a modern launch convenience, not a recovered original startup-seeding claim.
Restart retains the current race setup. No loadout algorithm or RNG draw order
changes were made.

Validation: core/Godot builds, core CTest 66/66, headless full competition
smoke and progress/boundary checks passed. Four real host startups verified two
distinct default seeds and two explicit seed-1 results.
