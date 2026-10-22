# Muzzle flash state and actual mesh ownership

SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`; retained raw evidence weapon_muzzle_visuals_ghidra.json.
This extends the five actual WeaponSalvoRuntime implementations, with no invented
original C++ class names and no shared RNG consumption.

Original nodes: MG 295ULFireLeft/Right; Gatling295ULFire1..5; Cannon,Plasma,Vulcan
295ULFire. The actual subtree is hidden in the persistent weapon model and copied
as a separate portable visual asset; its local origin is reset while original
children/geometry/UV/material flags are retained. No fake billboard replaces it.

All active flash timers lose3*dt. White RGB is retained and alpha=trunc(timer*255)
while visible and timer>0; otherwise hide. Inactive weapon callbacks hide every
flash and rearm. Timer and visibility reset on activation, but original roll/scale
persist. Machine0045b4ef decays timers BEFORE salvo launch0045b7f6, so a newly
fired flash has timer1/alphaff. Single-shot Cannon0040c919,Plasma0047d619 and
Vulcan004ceb79 launch before decay, so first displayed alpha is already reduced.

Gatling0044ce3c first decays all timers and applies visible-node scale3-2*timer,
then launches0044d12c, then sets alpha. Consequently first-ever new flash scale
stays its original1 until the next callback; reused nodes preserve their last scale.
The implementation retains this ordering rather than synthesizing an expanding
sprite with a different initial size.

MG rolls both muzzle nodes by180*dt (005f50a0=180). Getter004d5830(1) includes
parent roll, followed by a LOCAL setter, so native localroll gains parentroll AGAIN
every callback. This observed accumulation is preserved by the explicit parent_roll
argument. Gatling uses getter(0), so only180*dt is added to each local roll.

VehicleCombat exposes muzzle_flash visual kind, exact muzzle mesh index, composed
world pose, visibility and color_argb. SceneTurretCombat exposes muzzle_flash flag
and the same actual mesh/color/pose data, independently of projectile pool indices.
Live renderer instance keys must include visual kind to avoid flash/projectile alias.

Executed warning-enabled weapon_muzzle_visuals_test covers MG alternating launch,
prelaunch decay/full alpha, parent-roll accumulation and inactive cleanup; all three
single-shot first-alpha/hide contracts; Gatling scale-before-launch, local-only roll,
alpha and final hide/readiness. SceneTurretCombat synthetic firing and all-installed-
track asset tests pass with actual flash subtrees. No original pixel comparison.

