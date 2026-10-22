# Weapon trail lifecycle recovery

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static decompilation and selected assembly are retained in
`weapon_trails_ghidra.json`. The original executable was not run. Original source
and class names remain unresolved. `WeaponTrailPool` is a portable owner.

## Plasma continuous emitter

`0047d11e` constructs five shared projectile objects with the two emitter flags
both1. In `0047dd34`, the continuous emitter at projectile+2b8 uses mode0,
atlas2, spread3, inherited XZ velocity.75, inherited Y velocity.05, capacity
trunc(100*SPECIAL_EFFECTS_SCALAR), preload trunc(.1*scalar), size1 and the generic
Textures/16ParticleFX.bmp atlas. Its separate impact emitter is not this trail.
`0047e5e0` supplies current projectile position/velocity, emitting when the stopping
flag is clear, simulation scale1 and the actual callback delta. It advances before
contact testing and also during expiry cleanup. Existing `ParticleEmitter`
implements these generic lifecycle rules; no Godot particle simulation substitutes
for it. Three random draws are consumed per emitted Plasma trail particle because
the configured atlas is fixed, not randomized.

## Rocket specialized smoke

`0049f673..0049f68c` attaches the specialized emitter to the weapon spatial root,
not its flare child. `004f9037` initializes200 slots, size2.5 triangle geometry,
empty flag and Textures/xxParticleFX.bmp. This fixed capacity is independent of the
special-effects quality scalar. `004f928e` advances this distinct algorithm:

- On an emitting-state transition, reset previous source position to the current
  source. During emission add absolute X and Z displacement from the retained
  previous source; Y does not contribute.
- Subtract threshold1.850000023841858 repeatedly, cap the spawn count at3 and
  distribute those samples evenly from previous source to current source.
- If any samples were requested, retain current source and reset the distance
  accumulator to zero. If none were requested, retain the previous source. This
  repeated accumulation quirk is preserved.
- Each insertion succeeds only while active count<200, uses the next cyclic slot,
  consumes exactly one shared rand()%10, and inserts at the list head. There is no
  per-particle translation or rise. Spawn precedes aging.
- `004f969b` subtracts delta/1.75 from lifetime. Surviving particles accumulate
  the same scaled delta for animation. When that timer is strictly>.1, reset it
  to zero, increment the byte frame once and wrap values>3 to0. No catch-up loop
  advances several animation frames. Color is trunc(lifetime*255) alpha with
  RGB7f7f7f. Nonpositive lifetime removes the slot. The actual surviving list is
  the empty flag used by the moving weapon kernel.

The initial rand()%10 conflicts with `005007a1`, whose switch writes UV only for
frames0..3. For raw frames4..9 the original leaves a slot's previous UV untouched;
on the first use that UV is uninitialized. The portable pool retains known UV state
on reuse and exposes `atlas_frame(slot,out)`. It returns unavailable for an initial
undefined UV rather than inventing an atlas mapping. Particle snapshots retain
the raw animation frame for debugging. The next animation advance normally brings
the frame into the defined range. Renderers must honor the explicit UV query.

## Shared random stream and ownership

Nitrous configuration `00473d30` uses mode0, atlas3, spread3.5, inherited XZ.5/Y.8,
capacity trunc(150*scalar), size3, lifetime scale1. `0040ee2c` assigns vehicle44c4
from the exact `RearWeaponMount` node; `00473fa6` samples its world point and
subtracts normalized vehicle forward*.5. It advances this emitter after applying
the velocity increment, before timer increment and empty-pool completion checks.

Electron configuration `00433867` uses mode0, atlas0, spread10, inherited XZ.75/Y1,
capacity trunc(150*scalar), size1. `00434128` advances it on beam contact at the
actual contact position with the target vehicle's velocity and lifetime scale1.5;
`00433f1b` advances with emission false when no contact or after deactivation.

Smoke Screen is a third distinct pool: `004fa406`, `004fa546`, `004fa69f`,
`004fa9ea`. Capacity is fixed20. While emitting it adds elapsed to a retained
timer; when>=.2 it resets the timer to0 and attempts one insertion. No loop emits
multiple samples for a large delta. Insertion consumes atlas rand()%10 first,
then vertical-speed rand()%15/100. XZ velocity is original forward rotated by
heading-90 on even slots or heading+90 on odd slots, divided by3; initial XZ
position adds this velocity once. Life starts2 and loses delta/5, giving10seconds.
Animation uses unscaled delta with the same strictly>.1/reset/frame-wrap rule.
New particles insert at head and are updated immediately. Surviving particles
translate by velocity*delta. Their local triangle starts(-.25,-.75),(-.25,.25),
(.75,.25) and expands until its third X reaches12: first vertex adds(-6,-18)*dt,
second(-6,6)*dt and third(18,6)*dt. `screen_triangles()` exposes this actual
nonuniform geometry rather than replacing it with an isotropic size. RGB is999999,
alpha is255 while life>1, then trunc(life*255). The same explicit undefined initial
UV handling applies. Tests cover emission cap, RNG order/count, alternating direction,
geometry growth, UV recovery and expiry.

Guided subtype assembly `0044eca4` compares the string at00615f8c,
`THE EXCALIBUR`; `0044ed72` compares00615fb4, `SWARMERS`. Their continuous
generic emitters all use spread2, inherited XZ velocity.5/Y velocity1,
capacity trunc(150*scalar) and preload trunc(.1*scalar). Excalibur uses randomized
atlas-4 and size3; Swarmers uses atlas1 and size1; Guided Missile and Back Missile
use atlas2 and size1. `0044fc04` advances them at missile root position/velocity,
with emission gated by that missile's impact flag and lifetime simulation scale.75.
Thus Excalibur consumes four draws per particle, the others three. The corresponding
impact emitters remain separate. Tests cover Excalibur's draw count/size/lifetime
and Swarmers' fixed atlas/draw count.

The new callback constructor of `ParticleRandom` is infrastructure that borrows
the race's existing CRT draw function. It neither creates a second stream nor
reseeds. The existing seeded constructor and generator are unchanged. Callers own
one trail pool per original projectile or moving weapon; scene and vehicle keys
must remain distinct. The scene passes actual source transforms and queries actual
counts. Reset these pools together with their owning combat runtimes.

`weapon_trails_test` validates borrowed draw counts, Plasma quality capacity and
three draws per emission, actual expiration, Rocket distance cap/sample placement,
head insertion, vertical-only non-emission, initial undefined UV, frame recovery,
retained UV on reuse and invalid/unsupported requests. It and the existing
`particle_effects_test` passed in standalone warning-enabled builds.

## Generic impact bursts and blend state

`WeaponImpactPool` supplies separate generic impact emitters for five bullets,
Rocket and four guided variants. Machine Gun,20mm Cannon,Gatling Gun use atlas1,
spread5, inherited XZ.3/Y.05, base capacity15, preload.5 and size.25. Plasma uses
atlas2, base capacity50 and size1 with those same spread/inheritance/preload values.
Vulcan uses atlas-4 with Plasma's other values, plus a separate expanding sprite
exposed as `requires_secondary_sprite()` and implemented by `VulcanImpactSprite`.
Rocket uses atlas1, spread15, inherited XZ.35/Y.05, base capacity150, preload.1,
size1. Guided uses those latter spread/inheritance/preload values; assembly divides
150 by missile count and truncates BEFORE quality scaling. Excalibur uses atlas-4,
size8; Swarmers atlas2,size1; Guided/Back atlas-2,size1. Bullet lifetime scale is2;
Rocket/guided scale1.5, supplied to `begin`.

`begin` consumes no RNG. Original004e69a8 registers004e6a76 independently at the
shared callback tail; the callback invokes004e6271 with emittingtrue and the stored
lifetime scale. Captured initial callback count means a newly registered impact
waits for the next pass and continues after its moving weapon callback is removed.
The API does not invent a scheduler: its owner must preserve registration order.
Impact-event integration/global callback ordering remain required work.

Blend state is supported directly by004e689a,004f97a7,004fabd9 and queue renderer
004ffb96. Generic emitters set queued flag0, specialized Rocket/Smoke flag1. For0,
render states19=5 and20=2 mean SRCALPHA/ONE additive; for1,19=5 and20=6 mean
SRCALPHA/INVSRCALPHA. Alpha blending27=1, depth-write14=0 are set for submissions;
depth writes restore afterward. All use camera-yaw-only billboard matrices.

Flame's004ef747 loads16ParticleFX.bmp, but004ef996 and004efbee use UV helper005007a1
(the tire-format coordinates). Its triangle is size1.004efd55 setsqueuedflag0,
so Flame is additive despite using that alternate atlas-coordinate helper. Texture
family alone is therefore insufficient to select atlas coordinates or blend mode.

## Vulcan secondary expanding sprite

`weapon_vulcan_sprite_ghidra.json` corrects an earlier provisional debris description.
004f88f1 configures a single sprite using16ParticleFX2.bmp, atlas1 via005008cc,
size2 and growth80 (caller0047dd34).004f8e84 begins only when inactive, stores
position/velocity, sets life/RGB/scale to1 and registers an independent callback
after the generic impact emitter.004f8b7d subtracts4*delta from life and adds80*delta
to scale. If life>=0 it moves by velocity*max(life,.2)*delta and uses white RGB,
alpha trunc(life*255). Life strictly<0 removes the callback.004f8d1a combines the
yaw-only camera-facing transform with uniform scale, and queues flag0 (additive).
No random draw occurs anywhere in this secondary sprite lifecycle. Portable
`VulcanImpactSprite` exposes its scaled size directly in ParticleRenderState.
Tests verify repeated-begin rejection, movement/scale, zero-life survival and
subsequent removal.
