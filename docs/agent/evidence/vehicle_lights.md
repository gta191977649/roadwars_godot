# Vehicle lamp visibility

Read-only Ghidra and installation asset audit, 2026-10-22. Executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Raw decompilations: `vehicle_lights_ghidra.json` (local evidence).

## Confirmed behavior

- `0040ee2c` finds exact `295ULTailL` (string00612b20), storing its pointer at
  vehicle+44c0. `0040fc99` takes an enable argument, caches it at+4fbc and only
  calls visibility helpers on transitions. Missing meshes are tolerated.
- `004d289b` writes1 to mesh+5c; `004d28b0` writes0. Rendering004d314d gates
  submission on this field.
- Constructor0040d4cb at0040dd6a sets the cached flag1, then pushes0 at0040dd74
  before0040dd7c calls the setter. Thus stock lamps initially hide. Race reset
  `0048355e` also clears brake amount+4fb8 and calls the setter with0.
- `00424aa2` enables the lamp then stores/clamps service brake amount at+4fb8.
  Keyboard/controller dispatch00424d3e turns it off when no brake branch was
  selected. AI0041aa67 uses the same brake setter/release path. Remote state
  dispatch0046fe74 compares+4fb8 against zero and sets visibility accordingly
  (assembly00471519..00471543).
- Existing recovered keyboard resolution brakes when S opposes forward motion,
  or W opposes reverse motion. S at rest/driving backward requests reverse
  throttle, without braking. Light visibility must follow resolved braking,
  rather than raw S input, speed, reverse gear, or wheel rotation.

## Asset coverage and rendering

All12 stock Body/SpatialObject.msh files contain `295ULTailL`, flags295/0x127,
white material. These are unlit additive overlays with depth write disabled;
preserve the existing original material flags and textures. All12 also contain
`295ULheadlite` (Sarge: `295ULheadlite01`), using16Hlit.bmp,8vertices/4triangles.
The audited lookup/setter references control only the exact tail node; no
headlamp-name lookup, independent reverse-lamp node, or lamp configuration key
was found. Head overlays retain model visibility. This bounded search does not
claim recovery of every possible effect or a world-illuminating headlamp beam.

Tail meshes: Animal, Avenging Angel, Daisy, Niki Roadkill, Sarge, Wainright use
Brak.bmp/8vertices4triangles; Electra, Radcliff, Romulus use Brak.bmp/4vertices2triangles;
Heartbreaker, Kitiara use xxBrak.bmp/4vertices2triangles; Bud The Crud uses
xxBrak.bmp/12vertices8triangles. Kitiara also uses16Hlit.bmp in ordinary body
geometry, so texture-based lamp detection is incorrect. LOD2–5 have no named
lamp overlays; the current presentation uses the base body.

Sarge's untextured black body part1 contains NaN UV.u at bytes10596,11740,13764.
The portable reader now neutralizes nonfinite UVs only for untextured parts,
after reading the trailing texture identity. Positions/normals and textured UVs
remain strictly finite. This is renderer input sanitation, not a recovered
original NaN-repair algorithm.

## Reconstruction and validation

RacePresentation uploads tail geometry once, initially hidden, and switches
instance visibility only on changes. Each fixed simulation step supplies the
resolved VehicleState brake amount through update_wheels; no timer or per-frame
fade is introduced. The existing development handbrake forces brake_amount1
and therefore also lights the overlay; this development override is not evidence
of an additional original handbrake control. Restart clears handles and state.

The original-asset track_animation_probe checks all12 cars: initial off, reverse
without braking off, small positive brake on, repeated state without redundant
renderer calls, release off and cleanup/reload. Existing vehicle_dynamics tests
cover keyboard forward braking, reverse acceleration and braking reverse motion.
Original class/source ownership remains unresolved.

Executed validation: all12 original-car probe cases passed;29 portable CTest
tests passed, including malformed textured UV/position rejection and unused UV
sanitation. Godot4.7.2 extension rebuilt successfully. Native race-preview smoke
passed3 restarts/complete RID cleanup; native driving smoke passed
throttle/steer/brake/reset. These native smokes exercise the integrated path;
lamp transition assertions are recorded-renderer checks, not pixel-difference
tests. Progress validation and portable boundary checks passed.
