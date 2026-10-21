# Static-wall physics response subset

Binary SHA-256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra program `RoadWars`; retained output `wall_response_ghidra.json`.

`0042e8f0` orchestrates original car/track collision, including repeat collision dispatch,
physics response, effects, sound, profile/damage and networking. The implementation in
`wall_response.cpp` contains only its physics response between successful contact resolution
and remaining-timestep integration. The original function remains partial (`RE-STUB`),
not implemented or verified as a whole. Caller owns the repeat-contact loop (101 attempts).

## Physics observations

The solver has already committed a clear pose. Its normal Y is set to zero. Moving XZ
velocity is compared with that normal through normalized dot (`0047c1b4`), acos
(`00517334`), conversion to degrees (`005f53a0`), and conversion back to radians
(`005f539c`). The response uses absolute COSINE (`00517924`, explicit FCOS), not sine.
`00517334` endpoint assembly produces 0 for +1 and pi for -1, corroborating acos.

Moving response force is `normal * (2 * cached_full_3d_speed * mass * abs(cos(angle)))`.
`00414cf7(0)` reads the cached field, rather than recomputing speed after landing or
an earlier collision. The portable state preserves that explicit refresh boundary;
see `vehicle_cache_and_axes_ghidra.json`. A regression uses a cached speed different
from the current velocity magnitude.
If angle is below 90 degrees, or velocity has no XZ component, use normalized XZ normal
times `mass * 3` instead. Both branches call linear impulse (`0042bf6b`) and yaw impulse
(`0042c0c0`) with scalar timestep 1 and braking false. The latter uses the signed moment
helper `0047c21f` with fixed lever -1. The large-angle branch additionally scales velocity
by `1 - incidence` and yaw rate by 0.30000001192092896.

Finally, the solver's removed movement fraction is multiplied by the original dt;
the result integrates velocity and yaw from the clear pose. Failure-restoration contacts
restore previous pose and zero velocity as in `004328b5`. A nonzero XZ contact normal
is an internal invariant; the response asserts it rather than inventing a direction.

Constants read directly: `005f514c=2`, `005f51d0=3`, `005f51d4=90`,
`005f53a0=57.288352966308594`, `005f539c=0.017455555498600006`,
`005f5370=0.30000001192092896`.

`tests/core/wall_response_tests.cpp` checks synthetic head-on, glancing, stationary,
moving-away and restore cases. It tests the reconstruction contract only; no original
executable runtime comparison has been performed. Audio, damage and network hooks are
explicitly outside this diagnostic physics subset and remain unresolved.
