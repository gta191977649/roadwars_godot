# Scene turret combat integration

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static observations are retained in `scene_turret_combat_ghidra.json`; the original
executable was not run. `SceneTurretCombat` is a portable coordinator, not a
claimed original class name. Original source/class ownership remains unresolved.

## Confirmed object and callback contracts

`004c7c80` creates the turret's selected weapon. `004c8729` updates targeting,
rotation and firing conditions in their original order. Its firing helper
`004c8e7b` supplies the turret world origin, current yaw/pitch and speed60.
`RaceTurrets` owns that recovered controller behavior; the scene coordinator
consumes actual firing events rather than synthesizing attacks from timers.

The five bullet weapon attachment routines (`0045b305`, `0040c763`, `0047d456`,
`004ce9c3`, `0044cc4a`) attach the weapon spatial root to its parent. `004d2ea1`
resets the attached root's position and angles to zero; it does not erase muzzle
child positions. `004d3ee3` evaluates the world point. The loader therefore reads
the actual weapon mesh and exact named muzzle children from the original assets,
resets only the root position and uses the recovered turret transform. Unsupported
dummy basis paths, missing/duplicate muzzles and missing assets return unavailable.
Rocket collision dimensions come from its root mesh vertices, without children.

`004d724f` registers callbacks using `004edd40` tail insertion. Although `004d72a9`
captures the initial count, search mutates its shared list cursor: a new registration
can immediately execute the new tail, and head removal can skip the new head.
The previous blanket next-pass interpretation was false. Live integration now uses
stable per-controller/per-salvo/per-projectile tokens, synchronous observers for
registration/removal, and dispatch_callback. The owner-only advance method remains
a bounded reference harness. See combat_callback_cursor_ghidra.json for exact
cursor helpers and vehicle_combat.md for the integration boundary.

`004ba95f` still updates turret controllers during the initial weapon lock, passing
false to the attack gate while race time is less than the integer returned by
`00485372`. Equality enables this scene gate. The external `weapons_unlocked`
argument carries that original race-owned condition; the scene does not infer it
from player activation rules.

## Portable interface and limits

`load` reads the original scene controller collection, weapon parameters, weapon
meshes, ammunition meshes and muzzle/bounds data atomically. `advance` runs the
controllers, five recovered bullet salvo implementations or the detached Rocket
kernel, collision tests and synchronous contact damage dispatch. The caller's
damage callback receives the actual participant, contact side, damage and original
impulse contract and applies vehicle-owned effects. Read-only getters expose
turret poses, actual assets, projectile poses, launch/impact counts and damage events.

Plasma and Rocket require both real trail-pool callbacks. Missing callbacks fail
explicitly before advancing, rather than reporting a fabricated empty pool. Shared
random use, particle presentation, audio, muzzle flashes, explosion visuals and
external trail resets remain caller responsibilities. `reset` clears this scene's
runtime state; the caller must reset its corresponding external trail pools too.
The reserved portable scene shooter identity is validated against vehicle identities.
This integration does not claim the complete original scene manager or all21 vehicle
weapon implementations are restored.

## Validation

`scene_turret_combat_test` builds an in-memory original-format Cannon fixture. It
checks root attachment reset and exact muzzle position, an initial locked interval,
controller-selected firing, real projectile motion/vehicle collision and contact
damage reducing armor through `damage_vehicle_contact`, restart state and atomic
load failure. A non-cardinal target is intentional: the original yaw helper's
0/360 alignment quirk is preserved and separately covered by the turret test.

The optional original-asset profile loads each installed track world and all scene
turret assets, validating the exact muzzle paths and Rocket root bounds. Both the
synthetic test and this installed-asset load profile passed. The latter validates
asset interpretation; it is not an original-executable gameplay comparison.
