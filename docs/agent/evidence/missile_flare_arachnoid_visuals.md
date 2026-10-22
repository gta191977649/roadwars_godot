# Missile flare and Arachnoid articulated geometry

SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`. Raw functions/listings/constants are retained
in missile_flare_arachnoid_visuals_ghidra.json. No original class-name claim.

Guided initializer0044e926 and Rocket0049f460 locate exact child39ULmissFlar and
hide it initially. Actual launch0044f365/0049f83a shows the flare; detached missile
roots are actual SpatialObject meshes. The portable loader hides that child in
persistent projectile geometry, copies its exact subtree as an independent visual
asset, resets its standalone root translation and retains its true parent-local
position. Flare poses compose that local transform with actual projectile pose.

Assembly0045054c..00450582 reads Guided flare world roll and adds360*dt
(005f511c=360). Assembly0049fc06..0049fc2e does the same with30*dt
(005f5124=30). It is not the Machine/Gatling flash180 constant. Detached missile
root roll is0, so these are the actual local flare increments. Original launch
never resets flare roll: it persists across reused pool entries. Guided rotation
stops once its per-missile impacted flag is set. Rocket rotates on its final
cleanup callback as well. Visibility follows the actual detached projectile;
impact removal hides the whole parent hierarchy, with no floating standalone flare.

Arachnoid initializer0040667f locates exact Leg1 and Leg2 children. The original
004066eb modifies their LOCAL heading, with existing recovered phase/direction
state. VehicleCombat copies those actual subtrees, hides them in the parent ammo
mesh, and emits separate rear_leg visuals indexed instance*2+leg. Poses compose
real local leg position and recovered left/right heading with the actual moving
Arachnoid root. There is no guessed walk cycle, generated leg geometry or host-side
animation timer. Leg phase persists according to the real instance state.

All ammunition roots have serialized translation cleared for standalone assets,
because original launch overwrites that spatial root's position. Retaining it in a
renderer subtree would incorrectly add the file's root offset to the emitted world
position. Actual child offsets remain intact.

Executed warning-enabled guided_weapon_runtime_test now verifies360dt flare roll
and persistence across relaunch; weapon_projectiles_test verifies Rocket30dt,
final-cleanup roll and reuse persistence. Existing rear_weapon_instances_test
checks actual leg phase/heading/travel. VehicleCombat optional asset regression
loads every21 weapon family on every installed car, including required flare and
Leg1/Leg2 subtree references. Scene turret synthetic plus all-installed-track tests
also pass with actual Rocket flare geometry. No original pixel comparison claimed.

