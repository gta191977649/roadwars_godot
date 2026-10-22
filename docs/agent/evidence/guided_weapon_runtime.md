# Guided weapon motion and route guidance

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static decompilation is retained in `guided_weapon_runtime_ghidra.json`; constructor,
vtable and common parameter evidence is also in `weapon_runtime_ghidra.json`.
The original executable was not run.

## Ownership and entry points

Guided Missile, Back Missile and Swarmers use constructor0044e820/vtable005f5594;
The Excalibur derives through00434b90/vtable005f5408. Their original C++ class and
source names remain unavailable. `GuidedWeaponRuntime` is a portable unresolved
owner of the confirmed fields, not a claimed original class name.

Initializer0044e926 establishes at most four missile subobjects, their configured
AMMO_COUNT, ammo geometry width/height, ready+35c=1 and target+2d0=0. Readiness
virtual+2c is004352a0, returning that ready field; the shared guided-kind virtual+28
is a distinct predicate. Target selection0044f950 is reconstructed separately by
`select_race_missile_target`, using actual participant route order and owner
wrong-way state. The runtime receives the selected target position; it does not
invent a nearest-distance homing policy.

## Launch, tracking and completion

0044f365 requires inactive+270 and ready+35c. It resolves the selected target,
initializes tracking+2a4 from configured AMMO_TRACKING_TIMER_SECONDS and clears
each impact+320/jam+330 slot. The launch heading is incoming+10 for ordinary
guided weapons and incoming+170 for Back Missile. Horizontal/vertical scalar
speeds follow004cf5ed: speedY=speed*sin(pitch), speedXZ=speed-speedY.

For multiple missiles,004cf787 places even/odd subobjects at -.75/+ .75 times
the ammo model's width along heading+90. Indices2 and3 additionally move one
model height downward along the pitch-rotated up vector. These are geometry
derived offsets, not arbitrary swarm spread values.

0044fc04 advances horizontal and vertical speeds only while horizontal speed
is below AMMO_TOP_SPEED. It adds acceleration-abs(acceleration*initialY) to the
horizontal component and acceleration*initialY to the vertical component.
The initial direction is captured at launch, rather than replaced after homing.

Back Missile's launch phase is.1seconds; other guided types use.5seconds.
Tracking starts strictly after that phase. The shared tracking timer must be
strictly greater than tracking_seconds/count+tracking_seconds before it resets
and visits the missile slots. A nonzero jam timer decreases only during this
tracking pass. Impacted or still-jammed missiles do not consume homing noise.
Each eligible single missile consumes one shared rand draw for sign even though
its noise magnitude is zero. A multi-missile group consumes one magnitude draw
modulo8 and one sign draw per eligible missile. The same signed offset is added
to target X/Y/Z before original0047c694 heading calculation, which uses the full
3D normalized vector rather than atan2 on XZ.

Yaw follows the original shorter-arc, AMMO_YROTATIONAL_SPEED*dt step. Horizontal
movement uses this current yaw. During the launch phase, Y uses scalar vertical
speed; afterward Y descends at98units/sec and clamps to the sampled ground+.5.
The original query tests the newly moved XZ at current Y+5. The original missing-ground sentinel-99999 is a valid sampled height; a missing
service or failed query is an explicit failure, with no arbitrary fallback.

The runtime splits the original update into motion and completion phases so
the race can run its actual collision/explosion handling between them. Impact
clears that missile's jam timer. All impacted slots must also have zero live
trail particles before completion. Lifetime is strictly greater than5seconds,
or20seconds for The Excalibur. Clearing active does not immediately restore
readiness; the following callback performs hide/ready cleanup. These callbacks
must remain on the shared fixed60Hz simulation timeline.

## Excalibur route following

00434bd7 captures the owner's route index and branch. It chooses the immediately
preceding ranked participant, with no predecessor for the leader. The race
ranking owner supplies that target and its route index.004350b0 calls00434d5f
before ordinary guided movement.

00434d5f tests the prior missile segment against the next main/branch lap line,
then the previous line if no forward crossing occurred. It moves its own route
index at most one node and records whether the branch crossed. If the target
is fewer than two wrapped route nodes ahead, it returns the target position;
otherwise it returns004185fe(next route node, fraction0, current branch).
The kernel uses the already recovered RaceRoute sampling and preserves this
order. Missing route or target-route state fails explicitly instead of falling
back to ordinary direct homing.

## Validation and coverage limits

Warning-enabled GNU C++20 compilation and guided_weapon_runtime_test passed.
Fixtures cover launch/readiness, Back Missile heading/phase, four-missile
formation, homing draw counts, conditional jam decay, ground clamp, impact/trail
completion, expiry, Excalibur route-point/direct-target selection and unavailable
route data. The tests validate recovered contracts against retained static
branches; they do not establish original-executable equivalence.

Collision geometry, explosions, sound, trails and missile mesh ownership remain
the actual race/renderer implementation's responsibility. Broad original
functions retain partial RE-STUB mappings; emitting/moving a missile does not
claim a target hit or damage. The original post-cancel callback performs hidden motion, tracking RNG and trail
updates before cleanup. This ordering is now implemented and regression tested;
cleanup hides missiles, marks ready and resets age only after that work. Flare
roll is exposed and persists between launches. Live global dispatch, presentation
and audio integration are tracked separately in vehicle_combat.md.
