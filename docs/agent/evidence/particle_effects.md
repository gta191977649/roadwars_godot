# Original triangular particle emitter reconstruction

Date: 2026-10-22. Ghidra RoadWars, read-only analysis.
Binary SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Retained decompilation and selected assembly: particle_effects_ghidra.json.
Original class/source ownership is unresolved; portable files remain under unresolved.

## Constructor and stock vehicle sparks

`004e5e99` initializes a pooled triangular particle system; `0040e334..0040e36e`
configures the vehicle's emitter at offset 44f0:

| Field | Original argument/value |
| --- | --- |
| Mode | Burst, 1 |
| Atlas choice | 1 |
| Random velocity spread | 15 |
| X/Z inherited velocity | 0.1 |
| Y inherited velocity | 0.05 |
| Texture | Textures/16ParticleFX.bmp |
| Capacity | trunc(15 * SPECIAL_EFFECTS_SCALAR) |
| Preload integer | trunc(0.1 * SPECIAL_EFFECTS_SCALAR) |
| Triangle size | 0.25 |

Assembly `004e5f03..004e5f3a` proves both truncations. The preload field is
stored back as a float after truncation; it is zero at scalar=1. Do not replace
this with a guessed fractional emission rate.

`004e69a8` accepts a new burst only when the system is inactive and in burst mode.
It stores the origin/velocity, caches velocity times the current cached frame
delta and clears the next-slot index. Its emission accumulator is
capacity * 0.03 * preload_integer. The caller also supplies scalar 1.5 for wall
impact; this scalar accelerates lifetime/alpha aging, but not cached-step motion.
The call passes four explicit arguments while Ghidra's inferred prototype omits
the final unused delta. Assembly subtracts [EBP+0x10], the scale*delta argument.

## Exact lifecycle and numerical rules

- `004e6271` emits every 0.006666666362434626 seconds in burst mode, versus
  0.029999999329447746 in continuous mode. Bursts stop creating slots after
  reaching capacity; existing particles continue until dead. The original loop
  performs emissions before aging/removing existing particles.
- Cached frame delta starts at 0.03. Once an accumulator is strictly greater
  than one second, it resets to zero and caches the average of current and
  previous delta. New particles cache velocity times this delta. Existing
  particles retain their individual cached step.
- `004e655e` computes each velocity component as
  `(rand()%100 * 0.009999999776482582 * spread + inherited_component) - spread/2`.
  X/Z use horizontal inheritance, Y vertical inheritance. It subtracts
  velocity*0.10000000149011612 from initial X/Z only.
- Each particle starts with one second remaining lifetime. `004e67ea` subtracts
  simulation_scale times current elapsed delta, then if still alive adds its cached step once to
  position. It does not multiply displacement by the current frame delta and
  does not apply gravity, damping or acceleration.
- The draw color is white RGB with alpha trunc(remaining_life*255), copied to
  all three vertices. Zero/negative lifetime removes the slot.
- `005170b0` is the CRT generator state=state*214013+2531011, returning
  (state>>16)&32767. The portable test profile chooses an explicit seed; no
  claim is made about the original globally shared random-call ordering.

## Original geometry, atlas and native renderer contract

The triangle vertices are (-0.25,-0.75,0), (-0.25,0.25,0), (0.75,0.25,0)
times the configured size. `004e689a` uses a yaw-only camera-facing matrix;
it is not a fully spherical billboard.

`005008cc` supplies four explicit triangle atlas coordinate sets with values
0, 0.21799999475479126, 0.39100000262260437, 0.609000027179718 and
0.7820000052452087. `particle_atlas_uv` preserves those literal values. Negative
constructor atlas choice means select rand()%(-choice) per spawn.

`ParticleEmitter::particles()` exposes the complete slot state to the native
Godot renderer. Core owns emission, random state, movement and lifetime. Native
particle processing must display those slots without adding default gravity,
cone emission or alternate lifetime/motion integration. Coordinate reflection,
texture orientation and native GPU capability are adapter responsibilities.

## Reachable wall event bridge

`0047fff6` subtracts elapsed time from vehicle cooldown 42d8 and clamps it to
zero immediately before wall collision handling. `0042e8f0` emits an effect only
when cached speed is greater than 3 and that cooldown is zero; it sets cooldown
to 0.5 before dispatching the emitter. Origin uses the solved contact X/Z and
post-response vehicle root Y plus the original body local Y. Velocity is the
solver's normal. VehicleDynamics exposes these frame-local ParticleImpact events
without changing physical collision response.

`004dd8d4` is QMDX audio channel playback, not a particle function. It remains
outside the native particle bridge.

## Reachable tire smoke and surface dust

`00506485` configures each tire's independent pool with capacity
trunc(20 * SPECIAL_EFFECTS_SCALAR), Textures/xxParticleFX.bmp and initial color
0xffb2b2b2. Constructor random frame selection consumes one CRT random value per
slot; the visible update subsequently cycles frames in list traversal order.
The triangle is the same shape at size 2.5, i.e. vertices (-0.625,-1.875,0),
(-0.625,0.625,0), (1.875,0.625,0).

`0042b31a` copies the material record for each tire. Offset 0x10 is the dust
enable flag, offset 0x18 the smoke ARGB color; the dust flag is cleared when
cached speed is below one. `00413425` emits on each grounded tire when either
the cached slip angle is strictly between its geometric thresholds with cached
speed greater than two, or full braking is active with nonzero velocity, or
the surface dust flag is enabled. Airborne tires suppress both smoke and dust.
`00412834` derives the thresholds from the scaled tire X/Z bounds:
corner=atan(min_extent/max_extent)*57.288352966308594,
lower=corner/2 and upper=180-corner/2. The stock vehicle-specific SCALEX value
is read from Cars/Animal/Tires/Street/Poor/Data.pse, section SCALE.

The original effect source is the wheel world point raised by 0.1.
`005068a2` raises a new puff by another 0.2. `005066f2` emits at most one puff
per callback when the accumulated emission time reaches 0.1 and the tire source
is enabled. A successful spawn resets that accumulator to zero. Existing puffs
remain active when emission stops. `005069ca` subtracts delta/quality_scalar
from remaining lifetime, updates alpha while preserving material RGB and raises
Y by the cached rise step once per callback. It never changes X/Z. The rise
step starts at 0.0075 and, after a strictly-greater-than-one-second cache interval,
becomes 0.25 times the average of the current and previous frame deltas.

`005007a1` uses the bottom atlas row, V=0.609000027179718 to 1.0. The animated
frame counter increments modulo four for each active particle visited, in the
original AddHead list's newest-first order. Both portable emitters expose their
draw_order list to preserve this update ordering. Final original submission
passes through `004ffa25`, which sorts packets by depth; this differs from
pool slot order and should be represented by native particle view-depth draw order.
Tire submission `00506ae2` sets packet byte +0x60 to one, whereas sparks
`004e689a` set it to zero. Native material blending must honor the separate
texture/transparency pipelines. Queue consumer `004ffb96`, corroborated by
assembly at `004ffd80..004ffdb2`, establishes the flag's blend consequence:

| Queue setting | Original D3D state/value | Native equivalent |
| --- | --- | --- |
| Depth comparison | ZFUNC(21)=LESSEQUAL(4) | Depth test retained |
| Depth writes | ZWRITEENABLE(14)=0 | depth_draw_never |
| Alpha blending | ALPHABLENDENABLE(27)=1 | Transparent blending |
| Spark flag zero | SRCBLEND(19)=SRCALPHA(5), DESTBLEND(20)=ONE(2) | blend_add |
| Tire flag nonzero | SRCBLEND(19)=SRCALPHA(5), DESTBLEND(20)=INVSRCALPHA(6) | blend_mix |
| Fog | 004dbe9e sets FOGENABLE(28)=0 | fog_disabled |

The queue does not disable ZENABLE; it inherits the frame's enabled depth test.
After processing it restores depth writes and the frame fog state. Both
`004e689a` and `00506ae2` construct yaw-only billboards: matrix row 0 is
(cos(camera_heading),0,-sin(camera_heading)), row 2=(sin,0,cos), and Y remains
vertical. Flattening the handedness-adapted native camera right vector onto XZ
therefore preserves the intended orientation without adding pitch billboarding.
Retained queue decompilation and state observations are in
particle_render_states_ghidra.json.

## Damage smoke boundary

`004e7666`/`004e7dde` are a different smoke system using both 16ParticleFX and
xxParticleFX. Caller `004803e6` passes 1-health/max_health as intensity.
Emission requires that intensity to exceed 0.05. The undamaged free-driving
profile correctly has no damage smoke; no speculative permanent exhaust emitter
has been added. Weapon/nitrous/explosion sources remain outside this profile.

## Validation and boundaries

Standalone GNU C++20 particle_effects_test passes. It checks known CRT outputs,
constructor truncation, actual first-spawn velocity/position/alpha, cached-step
integration under a different elapsed delta, capacity, expiry, retrigger gating,
geometry and exact atlas coordinates. Steering regression separately passes.

Fresh full portable-core build and the extended 20-second original-asset driving
probe pass: three actual wall contacts produce three accepted spark bursts,
and the tire smoke/dust pools reach 40 active particles. The original vehicle
trajectory is unchanged by effect metadata. Tests also check tire capacity,
quality-scaled lifetime, preserved material RGB, upward motion, newest-first
animated frame order and exact lower-row UVs.

This restores the generic triangular emitter, tire smoke/dust and the reachable
undamaged vehicle/static-wall spark trigger. Damage, weapon and explosion
emitters and their game-specific enable predicates require further caller reconstruction.
Other emission types were observed at 0047dd34 and 0044ed08 but are not claimed
implemented by the vehicle-spark factory. Original callbacks/list ownership and
native render submission are host adaptations, so full functions retain RE-STUB.
