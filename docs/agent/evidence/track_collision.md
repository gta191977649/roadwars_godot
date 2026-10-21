# Original ground, spawn and static-wall reconstruction

Binary SHA-256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra program: `RoadWars`. Retained decompilation and targeted assembly:
`track_collision_ghidra.json`. Original source directory and class ownership remain unknown;
portable code is retained in `game_src/unresolved/track_collision.*`.

## Observations and implementation

- `004f2192`: 36-byte spatial-grid header, row-major counted cells, explicit record stride.
- `004f23c0`: absolute X/Z distance from signed grid origin divided by cell extent, then
  truncation and bounds check. Assembly restores divisions missing from pseudocode.
- `004bb23c`, `004fb769`, `004d59cf`, `004fb7fe`: first matching XZ triangle in the
  selected cell; triangle containment uses the original crossing predicate and height uses
  its stored plane normal. No raycasting replacement is used. Missing ground uses -99999.
- `00436270`, `004366f5`: 552-byte friction material records followed by triangle/material
  grid. Material IDs are one-based; material kind and the first three coefficients are
  retained. Audio handles and sound loading are outside this reconstruction.
- `004bb389`, `004bb573`: locate Start1 dummy, project its position onto original ground,
  subtract 0.5 from Y, and use the dummy heading. Mesh type-4 forward vector is compared
  against +Z with original conversion constant 57.288352966308594.
- `004bdecb`: static wall broadphase with height envelope and current/previous XZ distances.
- `0047cb51`: four body corners use diagonal radius, half-angle and front extension;
  pitch and roll arguments are unused in this function. Rotation constant bytes at
  `005f581c` are `f4 fe 8e 3c`.
- `004dd310` / `0047c368`: XZ segment intersection, including the original axis-aligned
  overlap cases. It is deliberately not replaced with a generic geometry library.
- `00430f70`: body-edge wall tests and enclosed-short-wall fallback.
- `00432002`: previous-position separation, then contact search and bisection with
  0.01/0.001 displacement-scaled tolerance; 200 separation corrections.
- `004317ad`, `004328b5`: contact collection and up to 100 compound-contact iterations;
  clear-position restoration, average contact and removed movement fraction.
- `00431a46` / `004315cd`: separating-side normal selection, increasing precision by
  bytes `17 b7 d1 38` at `005f53ac`. Original unbounded precision search is retained.
  Edge-normal fallback requires caller-supplied original vehicle backward/left/forward/right
  vectors; missing vectors trace/assert instead of substituting flat-world normals.
- `00431499`: shortest wrapped heading delta recovered from assembly; pseudocode omitted
  the return computation.

## Executed checks and limits

`tests/core/track_probe.cpp` reads the original installed Dark Forest assets using the
shared ResourceStore. It reports 15,288 surface records, Start1 position
`(153.592, 3.57632, 507.128)`, heading `326.004`, sampled ground `4.07632`,
normal `(-0.0139365, 0.999678, 0.0211873)`, friction `(0.9, 0.7, 0.05)`.
This corroborates file interpretation; it is not an original-runtime equivalence test.

`tests/core/wall_contact_tests.cpp` exercises no contact, edge contact, enclosed wall,
position bisection and wall normal on synthetic geometry. A unit-width footprint moving
from X=0 to X=1 against X=1.5 resolves to X=0.492188 and movement removal 0.507812.
These are reconstruction contract tests, not original-executable output comparisons.

Spawn hierarchy traversal currently accumulates translations only. The tested Dark Forest
Start1 has an unrotated parent; rotated-parent generalization requires original matrix
composition (`004d3ee3`) and remains unresolved. Lap progress and Lap.bin decoding are not
implemented. Static-wall contact does not implement vehicle collision impulse response,
damage, vehicle/vehicle contact, sounds or network effects; those are caller responsibilities.
Serialized malformed inputs are rejected explicitly. No function here is behavioral verified.
