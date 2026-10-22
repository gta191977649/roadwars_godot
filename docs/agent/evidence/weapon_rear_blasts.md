# Rear weapon specialized explosion pools

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static evidence is retained in `weapon_rear_blasts_ghidra.json`; original ownership
remains unresolved, and the original executable was not run. `WeaponRearBlastPool`
is a portable owner for two distinct emitter algorithms, not a guessed generic burst.

`0044df20` passes horizontal random modulus150 to `004f4fdb`; shared mine/Arachnoid
initializer `004567fa` passes40. Those caller observations are retained separately
in `rear_weapon_instances_ghidra.json`. Only Reactive Mines initializes the second
specialized emitter `004f641f`, consuming ten initial rand()%4 draws, one per slot.
This must occur at original object initialization rather than lazy first impact.

`004f5714` starts the base explosion only when inactive, retains the position,
clears created-count/timer and registers an independent callback. `004f5151` adds
delta to emission time; while>=.012500000186264515 it subtracts that interval,
creating slots until50 have been created. Created count is cumulative, not current
live count; a dying slot does not permit extra emissions. `004f52de` consumes five
draws in order: X speed, Y speed, Z speed, hue, initial atlas. X/Z speeds are
rand()%modulus/10 - integer(modulus/20); Y speed rand()%50/10+4. Initial XZ
position subtracts velocity/10; life starts1, hue=rand()%50/100, atlas=rand()%4.
New particles insert at the head before all particles update.

`004f55e8` subtracts2*delta from life; survivors move velocity*delta, consume another
rand()%4 in list order and set alpha=trunc(life*255), red255, green/blue
trunc((.5+hue)*255). No first-tick movement suppression is present. Empty list
and created-count>=50 end the callback. Each instance therefore creates exactly50
particles across its callback lifetime. Native renderer `004f5786` uses size1,
Textures/16ParticleFX.bmp, UV helper005007a1 and queued flag0 (additive).

Reactive extra initializer `004f641f` precomputes ten directions at36-degree steps,
size12 geometry and randomized atlas. `004f6a4f` registers this extra callback
separately after base registration. `004f687b` creates all ten slots on the first
callback without new RNG, inserting them at the head. `004f66f4` increments elapsed
and animation timers before update. `004f68f6` subtracts delta/10 from life; before
elapsed1.5 it translates by direction*delta, afterward direction*delta/10. While
the common animation timer is strictly>.1 each surviving particle consumes one
rand()%4; the timer resets once after the traversal. Alpha=trunc(life*255), RGBwhite.
After all ten expire it unregisters. Native004f6ace uses xxParticleFX.bmp,
UV helper005007a1 and queued flag1 (alpha blend).

The portable API exposes separate begin registration flags and separate advance
methods so the owner can preserve global callback order and shared RNG. It exposes
actual live particle/list snapshots. It does not combine the two callbacks into an
arbitrary per-weapon loop. `reset` clears runtime state without drawing again;
reconstructing an original object requires configure at its original load stage.

`weapon_trails_test` covers no creation draws for nonreactive initialization,
exact Reactive ten-draw initialization, base first-tick six draws, horizontal
speed modulus, cumulative50 emissions/expiry, Reactive list order, animation RNG
threshold and movement transition. Warning-enabled standalone build and test passed.
Native rendering, original runtime comparison and global cross-subsystem ordering
remain outside this kernel validation.
