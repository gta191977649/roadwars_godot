# Live portable Single Race combat

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only static evidence is retained in `race_session_combat_live_ghidra.json`,
the individual weapon ledgers, and `race_callback_registry_ghidra.json`.
Portable owner names remain unresolved; no native executable was run.

## Object ownership and phases

`00480b40` dispatches the selected controller before the participant force loop.
Each other AI controller, including its `0041a1cd` weapon predicate callback,
runs immediately before that participant's forces. The portable implementation
does not batch weapon requests after all integrations: that changes both sensed
positions and the shared callback registration order. A read-only dispatch trace
supports an original-order regression asserting selected controller first, then
each other controller followed by its own force phase. `0041aa67` continues its
driving calculations and route nitrous checks for finished actors before its
finished gate suppresses controls and weapon AI. Selected completion switches
that controller to AI, while the race keeps updating until its menu is requested.

Guided target ownership is split as in `0044f365`: AI virtual1c updates common
weapon+08 query target. Actual guided begin calls `0044f950` again and copies
the result to+2d0, which remains latched for that activation. Guidance follows
that identity's current pose, not later changes to the AI query pointer.
Passive character JAM intentionally checks+08, preserving its distinct native
eligibility quirk. The session trace/debug data expose both identities; tests
assert an active weapon with unchanged ammunition cannot retarget mid-flight.

The race owns participants, scene controllers, and the shared CRT state. Each
vehicle owns its actual `VehicleCombat` and `VehicleDamageVisuals` objects.
The host supplies input and renders read-only views; it does not implement weapon
damage, item motion, AI predicates, or a second competition particle simulation.
Actual canonical descriptors normalize original uppercase equipment table names
before invoking case-sensitive kernels.

The original vehicle update/health phase precedes direct world effects. Health
particles, tire particles, passive character JAM, and repair/death state are
updated per participant. Passive JAM uses `[PRICE_SCALARS] JAM`, visits every
other active side weapon targeting the character, and tests every projectile
position, including hidden ones, with strict XZ distance <30. Each match jams
the whole weapon for1 second, retaining duplicate calls and consuming no RNG.
Character TRACTION multiplies longitudinal/lateral coefficients after oil and
disabled-contact overrides; rolling resistance is not multiplied. The native
disabled-contact field+1c is distinct from death timer+134.

Chimneys, turret controllers, and scene Electron activation run in the direct
world phase. Chimney construction caches the maximum ViewDistanceMap distance,
without the global scalar; each ordered Stack has its own pool. The camera is
updated after this phase and before the shared global callback dispatch.
See `race_callback_registry.md` for the unusual mutable-cursor semantics.
All live effects borrow the same CRT generator. Reactive Mines consume their
five eager ten-draw radial initializers synchronously inside the REAR equipment
installer, before subsequent armor/drivetrain selections.

`0041de2a` applies pair health/paint/deformation and physical response, then starts
the selected owner's collision sparks before removed-time reintegration.
`0042e8f0` passes the actual wall response vector times.75 to `004215fd` only
on its first iteration, after the spark phase. Health retains difficulty scaling;
visual severity receives the unscaled vector. The physical pair impulses are
not multiplied by death-only damage-vector factors.

Accepted selected internal damage emits the elimination notification at the
positive-HP to zero transition, as in `00414d2a`, before the later death-timer
update. AI death and manually assigning an already-zero HP state do not invent
this cue. Direct weapons, armor bypass, and collision armor overflow share the
same portable notification boundary. Mutable paint surfaces are aliased by
canonical car/paint texture path, preserving the original texture cache sharing;
mark counters, geometry, and deformation remain per vehicle.

Mounted weapon and HoodPoint/RearWeaponMount poses use actual hierarchy Euler
sums, including the body spring pitch/roll and recovered per-node overrides.
Serialized dummy heading/basis metadata is not treated as an original live Euler
rotation. The same pose source feeds emission and presentation.

## Finish and idle menu

`00480b40` marks the selected participant finished only when its completed lap
count reaches the configured total. `0047fd27` locks the finished prefix in
participant scan order and ranks the remainder by laps and route progress;
death neither excludes a participant nor invents a finishing place.
`0045c29c` treats either selected finish or nonzero death timer as terminal for
the offline Single Race menu: any held physical keyboard key resets a10-second
timer, the frame step is subtracted, and strict timer<0 synthesizes Escape.
`0045d8ed` chooses the Single Race Over menu for either state. World simulation
continues during that interval. Rewards, replay saving, and persistence remain
outside the authorized reconstruction scope.

## Explicit limits

The seed and reduced participant count are diagnostic options. Fresh Single Race initializes caller option+28 to1, selecting20 seconds of
weapon lock through `004aac8f -> 004a4f79 -> 00484189`. The diagnostic option
`diagnostic_weapon_lock_seconds` defaults to that confirmed20 and retains an
explicit0 override. Activation uses the native
strict comparison, while the HUD uses the distinct <= comparison. Fresh vehicle construction `0040d4cb` writes+4ab0=1; `00487fbe` therefore
starts the HUD at map0 and its cycle returns from6 to0. Native weather previous-camera fields are uninitialized in the
constructor; the portable boundary explicitly seeds them with the actual initial
camera, without claiming the original first frame has been established.
Native particle branches with undefined initial atlas state expose unavailable
UVs while continuing their recovered simulation/RNG; rendering must not invent
a frame. Native spatial audio attenuation/Doppler is outside the current adapter.

Tests execute the reconstruction. Successful builds and deterministic rendering
cadence regressions demonstrate portable consistency, not native behavioral
equivalence. Individual ledgers retain unverified/missing native behavior.

## Executed integration acceptance

The final original-asset `roadwars_race_session_test` profile (Dark Forest,
Animal, five AI, one lap, diagnostic seed1, native lock20) passed at10,15,30,60,
75,120,144,240 rendering FPS and350/900ms catch-up intervals. All7157 recorded
fixed ticks matched bit-for-bit, including equipment/ammunition/actual elapsed
timers, health/armor, query and latched targets, AI state, particles, route/rank,
damage records, camera, and the shared CRT state. It then verified world updates
continue after terminal state. The profile also checks the native selected-first
controller/force order, active-guided target stability, actual collision-caused
selected elimination and notification, ten-second menu timeout, keyboard reset,
pause, restart, and repeated seeded initialization.

The final original-asset `roadwars_race_session_combat_lifecycle_test` passed all
21 branches selected through unmodified original loadout tables with diagnostic
seeds1..12. It checks the real lock, zero-tick press/release retention, actual
ammunition use, pause/RNG freeze, catch-up debt, in-flight restart, refire and exit.
These are portable behavior regressions against static evidence; native runtime
comparison, undefined native initial UV/history state, and the stated output
adapter limits remain explicit.
