# Stack chimney particles

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static recovery only; original source/class name unresolved. `ChimneyParticles`
is a portable owner, not a recovered original name.

World loading collects prefix Stack (length5) nodes, constructs one emitter per
node at world+79f4, and stores positions at79f0. The world+79e8 field is an enable
flag. Assembly `004baac5..004baad1` confirms ECX comes from that per-node emitter
array; the decompiler omits this receiver. `004ba95f` visits them in node order
before turret updates. Each pool has10 slots; configuration constructor004fb870
and slot constructor004fc0e0 consume no random draws. Start004fba48 resets emission
timer and clears stopped flag; it does not clear existing smoke.

`004fbaad` square-culls using strict abs(X difference)<range AND abs(Z difference)
<range, where range=native field root+8f0 times float1.2. Out-of-range freezes all
particle state and emission time. In range, emission timer advances and >.5 emits
one particle when enabled, then resets timer. The .5 constant is a DOUBLE at
005f54c8, confirmed by FCOMP double at004fbb3e. No catch-up emission occurs.

`004fbc3f` creates at the exact node point, life1, animation timer0, frame rand()%4
(one shared CRT draw), newest-first list. `004fbe36` subtracts delta/3 from life;
life<=0 removes. Survivors rise by delta, color alpha trunc(life*255), RGB999999.
Animation timer >.1 advances once and resets0. MFC GetNext advances iterator before
the update, so removal never skips another particle. Triangle vertices start
(-.625,-1.875,0),(-.625,.625,0),(1.875,.625,0); per-delta expansions are
(-.75,-2.25,0),(-.75,.75,0),(2.25,.75,0). Rendering is yaw-only, xxParticleFX.bmp,
005007a1 tire UVs, native alpha material as shared particle evidence records.

Warning-enabled standalone C++20 chimney_particles_test passed, testing the .5
strict boundary, frozen range state, one draw per spawn, lifetime, expansion,
existing/new particle updates and removal. Caller must use the actual camera and
native view-distance configuration; a guessed distance changes gameplay RNG.
