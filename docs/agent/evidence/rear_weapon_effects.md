# Original rear weapon effect kernels

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only RoadWars Ghidra evidence is retained in `rear_weapon_effects_ghidra.json`.
No original executable was run. The portable field views and owner types are not
claims of original C++ class names, namespaces or native object layouts.

## Ownership and virtual dispatch

Factory00410ea2 confirms the seven named constructions below. Constructor bodies
write their respective vtables. Raw little-endian vtable pointers resolve through
E9 thunks; bytes/targets are retained. Virtual+14 is activation; owner update methods
are registered separately. Generic base activation004cf5ed and deactivation004cf710
remain shared weapon runtime behavior and must execute with these kernels.

| Original name | Constructor | Vtable | Activation | Update | Recovered kernel | Remaining coverage |
| --- | --- | --- | --- | --- | --- | --- |
| Missile Jammer |00464f50|005f573c|00465217|00465395|Readiness, radius, duration, visual draws and weapon jamming|Native audio/mesh callbacks and integrated lifecycle |
| Oil Slick |00474270|005f57ac|00474472|004746b7|Four-use gate, emission timing, collision/render triangles, route-pruned wheel contacts and grip override|Native projected mesh upload, audio and integrated effect timing |
| Smoke Screen |004a6110|005f5960|004a622b|004a62b5|Three-second emission/cleanup gate|Original particle pool004fa546 and native presentation |
| Grenades |0044db70|005f5534|0044dcd7|0044dda1|Five-slot cyclic deployment owner dispatch|Individual grenade launch0044e05f, movement, collisions and explosion |
| Land Mines |004563d0|005f5660|00456549|00456617|Five-slot non-reused deployment owner dispatch|Individual mine arming, collision and explosion |
| Reactive Mines |0049ce70|005f5894|0049cfe9|0049d0b7|Five-slot non-reused deployment owner dispatch|Individual reactive tracking, collision and explosion |
| Arachnoids |00406320|005f5024|00406499|00406567|Five-slot non-reused deployment owner dispatch|Individual seeker movement, collision and explosion |

These remain partial reconstructions. Back Missile and other guided families are
handled by their distinct guided runtime, not merged into these seven effect types.
An absent actual deployment callback returns unavailable; no generic projectile
model is substituted for missing grenade, mine or arachnoid behavior.

## Missile Jammer observations

`00465038` initializes radius+538=30, elapsed+53c=0 and ready+540=1. `00465217`
requires active+270==0 and ready!=0, calls common activation, then clears elapsed
and ready, attaches the effect mesh to the original scene and starts sound.

`00465395` consumes three shared RNG draws each actual update: grey=rand()%10*.1,
RGB=trunc(grey*255) with alphaff; rand()%2 selects heading subtraction/addition;
rand()%270 supplies the amount. It follows the owner position/pitch/roll. The
portable output retains grey/heading and takes all three draws explicitly.

For every other chassis identity, it visits every projectile position of the side
weapon (float count+288). XZ distance must be strictly<radius. Each qualifying
projectile calls `004cf76b(1.0)` on the entire weapon, which invokes virtual+3c.
Guided vtable005f5594 virtual+3c resolves to0044f81d: every projectile jam timer
at+330[i] is set to1. Duplicate close projectiles therefore yield duplicate whole-
weapon commands. No invented target, health, height or active-projectile filter
is added. Actual guided timer decay/homing belongs to0044fc04.

Active updates add dt to elapsed and deactivate only after elapsed>5. This does
not immediately rearm. The following inactive update clears elapsed, sets ready1,
removes visual callbacks and stops sound. That final update still consumes its
three visual RNG draws and scans projectile positions before cleanup. The host
must preserve the original callback lifetime and shared RNG order.

## Oil Slick observations

`00474472` permits activation only when inactive and current patch+2dc<4. It
clears elapsed+2d0/sample+2d4, increments patch count+2d8, activates the common
weapon, stores owner main-route node and current original position. Five patch
storage slots exist, but the gate allows four completed deployments.

`004746b7` increments both timers each callback. Elapsed>.9 while active calls
deactivation and increments current patch before considering sampling. Inactive
updates remove the callback. Active sampling requires sample timer>.2 OR
abs(previousX-X)+abs(previousZ-Z)>4. Sampling stores current position and resets
sample time even when its50-triangle capacity is full.

For the supplied original heading-rotated forward=(x,0,z), offsets are
a=(6z-6x)/2, b=(-6z-6x)/2, c=(6x-6z)/2. The first triangle is [position,
position+(a,0,b), position+(b,0,c)]. `004bb23c` projects all three Y coordinates
onto track surfaces. That triangle is stored for collision. Only afterward the
second and third vertices receive the same XZ offsets again and all vertices
gain Y+.09 for rendering. The portable kernel preserves this observed difference
instead of silently making render and collision triangles equal. Projection is
an explicit required service; no guessed height is used.

`00474bb5` scans each patch only when abs(vehicle node-patch node)<3 OR
abs difference>main-node-count-4. It tests all four cached wheel XZ points in
every triangle through004d59cf, counting every match without clamping overlaps.
`00474cf5` writes each vehicle+08 from this count. `0042b31a` tests count>0 and
cached speed strictly>5, then assigns ALL four longitudinal/lateral/rolling
coefficients.05 before subsequent health/difficulty multipliers. It does not
reduce only the wheel that touched the patch.

## Smoke and deployment observations

Smoke activation004a622b resets+c88 before common activation. Update004a62b5
adds dt, deactivates at strictly>3, removes its callback when its+c84 flag is
nonzero and the common active flag is zero, and supplies the resulting active
flag to the original smoke particle pool004fa546. The kernel does not invent
particle behavior or a driving penalty.

Grenade owner0044dda1 decrements timer, launches its current instance when active,
increments the index and wraps after4, removes callback, sets timer1 and deactivates.
Land/Reactive/Arachnoid owners instead increment timer and launch only while index<5,
then increment without reuse. They remove callback and deactivate even at capacity.
Their distinct actual instance-launch callbacks remain mandatory.

## Executed validation and limits

GNU C++20 warning-enabled standalone build and `rear_weapon_effects_test` passed.
Coverage includes jammer strict radius/timing, XZ height invariance, self exclusion,
duplicate per-projectile commands, visual RNG-derived outputs, delayed rearm and
all-projectile jam assignment; oil strict timing/travel/route gates, all four uses,
projected collision/render differences, capacity50, overlapping wheel counts and
speed5 coefficient boundary; smoke expiry; grenade ring versus finite instance
pools; invalid input and explicit unavailable projection/instance behavior.

These tests exercise recovered kernels with supplied original field views. They
do not establish integrated gameplay, audio/pixel equivalence or original-executable
behavioral equivalence. Full per-instance rear weapon behavior remains open as
listed above; do not promote the containing original functions to complete.

The passive character jammer00415761 explicitly compares side-weapon+8 (the
query target) with this vehicle, then checks+270 active and each projectile.
It does not compare the guided launch's separate+2d0 latched target. Therefore
changing AI query selection during flight can change passive-jam eligibility
without retargeting the flying missile. The live adapter preserves that distinction.
