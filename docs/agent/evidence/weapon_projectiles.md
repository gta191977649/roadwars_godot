# Shared pooled projectile motion and hit selection

Reference SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static Ghidra records: `weapon_projectiles_ghidra.json`. No original executable
was run. Projectile object ownership is confirmed by the weapon-owned arrays,
owner link +2b0, and callback registration; original C++ class names remain unknown.

`0047e332` launches only an inactive pool slot. It rotates +Z around the original
negative-X axis for pitch, then +Y for heading. Those global vectors are initialized
by CRT functions `004dab4a`, `004dabb0`, `004dabd2`, confirmed in retained assembly.
The original radians constant `005f5884` is float .017455555498600006. The launch
sets velocity from the supplied speed, acceleration vector from weapon+290,
current/previous positions from the muzzle, scalar speed and heading, age zero and
active one. Vehicle velocity is not added to the projectile by this function.

`0047e5e0` increments age and scalar speed first. The age limit constant at
`005f50a4` is FIVE seconds. While age <=5 it saves previous position and adds
acceleration*dt to velocity only if the already-updated scalar speed is below the
weapon maximum. It then adds velocity*dt to position. It does not clamp speed,
apply gravity or calculate a partial acceleration at the cap. Expired projectiles
stop without moving. Trail projectiles retain their callback until their pool
drains; `0047e529` sets their stopping flag to suppress additional collisions.

`0047eb01` checks target identity against owner identity and scans original
participant order, then target edges in order. It returns the FIRST matching
participant/edge, not the nearest intersection. The strict Y gate is absolute
current-projectile-Y minus target-origin-Y <.5 below/equal origin, or <target
height above origin. Segment tests use XZ only; returned Y is the current projectile
Y. Edge 0,1,2,3 map to collision sides 0,3,1,2. The generic geometry helper returns
the event; each actual weapon hit handler must apply its own original damage and
impulse behavior. Dying targets are not silently removed from this geometry scan.

`0047f11f` runs AFTER vehicle detection. `004be17b` sweeps each projectile footprint
corner from previous to current against broadphase wall segments and accepts only
movement into the wall normal. It uses the existing original body-corner helper
and axis-overlap XZ intersection predicate. Wall hits return current projectile
position rather than an invented earliest contact. Otherwise `004bb23c`'s ground
test hits when the current position is at or below sampled original ground. The
TrackCollision overload uses its original wall broadphase and surface sampler.

These helpers recover motion and contact selection. Weapon-specific muzzle
transforms, callback order, impact particles, sound and trail pools require their
own integration. They are not implemented by
substituting generic bullet behavior or a Godot raycast. Broad original function
statuses remain partial/analyzing.

`weapon_projectiles_test` validates original acceleration/cap order, five-second
expiry, positive pitch direction, pool reuse, participant/edge priority over
nearest-hit order, strict vertical boundaries, owner exclusion, wall facing,
ground equality and trail drain behavior. These are reconstruction contract
tests, not original-runtime equivalence.

## Five confirmed projectile weapon salvo drivers

`weapon_projectiles_salvo_ghidra.json` retains each actual fire/update/launch/hit
function and initialization parameters. `WeaponSalvoRuntime` is a portable owner,
not a recovered original class name. Unsupported weapon initialization returns
`Result::unavailable`; Rocket Launcher is not substituted with a bullet.

| Weapon | Pool | Cadence and duration | Original muzzle | Initial projectile speed |
| --- | --- | --- | --- | --- |
| Machine Gun | 15 | timer starts .2; one shot when >=.2, reset zero; stop elapsed>.9 | 295ULFireLeft / 295ULFireRight, alternates starting left | shooter speed+60 left, +40 right |
| 20mm Cannon | 5 | one shot while elapsed==0; stop elapsed>1 | 295ULFire | shooter speed+90 |
| Plasma Blaster | 5 | one shot while elapsed==0; stop elapsed>1 | 295ULFire | shooter speed+110 |
| Vulcanized Gun | 5 | one shot while elapsed==0; stop elapsed>1 | 295ULFire | shooter speed+70 |
| Gatling Gun | 30 | timer starts .07; one shot when >=.07, reset zero; stop elapsed>.9 | 295ULFire1..5, cyclic starting 1 | shooter speed+60 |

Pool indices persist across salvos; barrel indices reset at firing. Single-shot
drivers launch before incrementing elapsed. Burst drivers increment elapsed and
cadence before launching, then check the duration cutoff. No loop creates several
shots after a large callback and no fractional cadence remainder is retained.
The live fixed-step scheduler therefore supplies the intended stable 60Hz cadence.
After stopping, one callback performs cleanup before the weapon is ready again.
The caller must take each muzzle's actual world transform, including all mount
and weapon hierarchy transforms. A guessed body-center muzzle is unsupported.

All five hit handlers use the same damage routing implemented in
`damage_vehicle_contact`. Their original network notification wrapper remains
separate. Only Vulcanized Gun sets the projectile impact-impulse flag. Its force
uses horizontal yaw direction times modulus(full 3D projectile velocity) times
ammunition mass in kilograms times impact multiplier. The application point has
hit XZ and zero Y. Call the existing `apply_vehicle_impulse` with multiplier1 and
braking=false, then `apply_vehicle_yaw_impulse` with dt1, braking=false and default
computed lever. The original does not call tilt in this branch. Plasma's pool has
a persistent trail; the other four have no persistent trail. All five initialize
wall radius .1, footprint diagonal .1 and corner angle45 degrees.

Additional tests cover initial and repeated salvo requests, original muzzle
alternation, pool persistence, cleanup readiness, single-shot durations and
speed offsets, Gatling's five barrels, explicit unsupported initialization and
the Vulcan impulse's full-speed/horizontal-direction distinction.

## Rocket Launcher has its own moving weapon kernel

`weapon_projectiles_rocket_ghidra.json` retains the Rocket Launcher constructor,
initializer, fire, update, world/vehicle contact and hit handler plus smoke-pool
emptiness computation. It is not a shared bullet salvo. `launch_weapon_rocket`
uses the supplied starting speed directly, with no bullet speed addition, and
launches one detached weapon object. Initial horizontal scalar speed is speed
minus speed*direction.Y (without absolute value); initial vertical scalar speed
is speed*direction.Y. `0049faaf` adds direction*acceleration*dt to full velocity,
adds velocity*dt to position and updates horizontal scalar speed by
(acceleration-abs(acceleration*direction.Y))*dt. There is no top-speed clamp in
this function. Direct impact uses that horizontal scalar speed for its impulse,
not full velocity modulus.

`004a00d2` uses the same participant and edge ordering as bullets but substitutes
rocket mesh height for the below-target .5 gate. `0049fe9a` uses bounds from the
actual rocket mesh: diagonal from half width/half length, original atan corner
angle and full mesh length for broadphase radius. The caller must supply these
actual mesh bounds; no guessed dimensions are provided by the portable kernel.
The original direct-hit function `0049ff95` has the same armor/internal routing
as the five bullet weapons. The impact explosion call `004e69a8` is the existing
visual burst pool; these Rocket Launcher functions contain no radial damage
scan. Inventing radial damage from the appearance would change the game.

After collision, active clears but the motion callback remains while smoke
drains. The callback moves first even on the final cleanup tick. On a tick that
begins inactive with the smoke-pool empty flag true, it unregisters, marks ready
and resets elapsed. The original smoke object starts at rocket+58c and stores
its empty flag at +4c5c, matching rocket+51e8; `004f928e` writes that flag using
the actual pool IsEmpty result. The caller's `trail_empty` must reflect this
pool. Five-second expiry stops emission but can still allow contacts during
smoke drain until an actual collision occurs. Rendering/audio/smoke updates are
separate from the recovered moving-weapon kernel and are not claimed complete.

Rocket tests cover direct launch speed, acceleration and first-contact routing,
horizontal-scalar impact impulse, post-impact cleanup movement, pitch-dependent
speed components and expiry with a nonempty smoke pool.
