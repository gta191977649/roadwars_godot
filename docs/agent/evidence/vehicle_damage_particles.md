# Vehicle damage particles

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
This is static recovery, not original runtime observation. Original class/source
names remain unresolved; `VehicleDamageParticles` is a portable owner.

`004e7510` constructs forty stride0x78 slots using `004e8190`. Neither that
constructor nor its empty point constructors `004180a0` consumes random values.
`004e7dde` receives damage fraction, source, vehicle velocity and delta directly
from the vehicle health phase. Source/camera offsets belong to that caller.
The emission accumulator runs even while disabled. At >=.1 seconds, enabled and
damage>.05, it resets the accumulator and tries to append exactly one particle.
There is no catch-up loop. Capacity is40, independent of special-effects scalar.

`004e79c6` consumes three shared CRT draws: X and Z velocity jitter each
`rand()%100/100-.5`, then initial atlas frame `rand()%4`. Source is moved back by
vehicle velocity*delta; Y velocity adds4. Initial lifetime is min(damage,1), not
a fixed1. White base RGB changes below .7 initial life to (life/2,life/2,.2).
Color alpha is remainingLife*255; RGB brightness is1 above .7 and life*.5 otherwise.
`004e7770` subtracts delta, removes only below zero, and moves surviving particles
by velocity*max(remainingLife,.3)*delta. Animation advances one frame at >.075.

The custom newest-first list and dynamic loop bound in `004e7dde`, including
head-removal skipping and count shrink, are preserved. Start/clear/drain follow
`004e7fa0`/`004e7fdb`; draining alone does not gate emission.

Presentation: yaw-only asymmetric triangle vertices (-.625,-1.875,0),
(-.625,.625,0),(1.875,.625,0), using `005007a1` tire UVs. RemainingLife>=.7 uses
Textures/16ParticleFX.bmp; lower life uses Textures/xxParticleFX.bmp. Corresponding
native materials retain additive/alpha contracts recovered in the common effects
evidence. `004e7c35` writes `[emitter+28]=0` without slot indexing. This is slot0's
animation timer; the other39 timers are never initialized by `004e8190`.
The portable implementation explicitly reports their atlas frames unavailable,
while continuing exact known lifetime, motion, color and random consumption.
It does not silently seed those native indeterminate timers with zero.

`vehicle_damage_particles_test` checks emission gates, retained timer, shared draw
count, initial/moving color and velocity, material transition, native list removal,
stop/start and explicit unavailable atlas state. Warning-enabled standalone C++20
build and test passed. This does not establish original runtime parity for the
indeterminate presentation state or caller camera transform.
