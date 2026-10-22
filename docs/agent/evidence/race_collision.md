# Vehicle pair collision presentation-independent reconstruction

Reference SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
All observations are static. The original executable was not run. Primary
decompilation is retained in `race_session_ghidra.json`; additional helper bodies,
response call assembly and constants are in `race_collision_ghidra.json`.
The signed-angle helper's omitted decompiler tail is retained separately in
`race_collision_angle_listing.json`.

## Confirmed geometry and update sequence

- `0041d580` forms a length/width rectangle, rotates X/Z using float
  `005f52d4 = .017455555498600006` and translates it. `0041e5bd` flattens Y and
  compares all 4x4 edge pairs through `0047c368` / `004dd310`. Pure containment
  without edge intersections is not an invented additional collision predicate.
- `0041e3a6` prunes by route-node difference, full position distance against
  both lengths plus 2, then vertical extent. Route pruning belongs to the caller;
  distance and height guards are in the pure pair solver. Heights are the
  original length-versus-height sine expression, not a new bounding sphere.
- `0041e5bd` retreats both poses along their just-completed displacement and
  signed heading change. `0041d74d` builds candidate rectangles. The divisor
  `005f52e0` is **1.9500000476837158**, not 2. A clear candidate after more than
  one iteration stops the search; the bound is 20 candidate calculations.
- `00431499` has misleading decompilation because the floating return and tail
  were not recovered. Its disassembly confirms wrapping to [0,360), a signed
  shortest-arc difference, division by the integer sample count, and negation.
  The pair call negates its result again before rollback.
- The bounded failure branch uses negative first-car displacement times the
  second-car mass, at the first car's saved position. It is preserved separately
  from the successful edge-normal response.

## Confirmed impulse subset

`0041d92c` normalizes an edge normal and tests both endpoints of the other edge
against it. `0041d9d4` selects a fully separating normal first, otherwise the
candidate with the greatest endpoint cosine, preserving record order.
Intersection records retain indices corresponding to the original candidate-array
edge pointers: the last intersection points survive a clear candidate, while the
edge endpoints reflect that final candidate's updated geometry.

`0041e180` obtains contact velocity for both cars at the first recorded contact
point (`0042ba96`), multiplies each velocity component by the selected normal
component and mass, subtracts these vectors, and takes their modulus. The force
is the selected normal times this magnitude; this is not a substituted dot-product
or restitution solver. The selected record's point supplies the yaw lever.

`0041de2a` disassembly confirms equal/opposite translation at .65 and yaw at .6,
using existing recovered impulse helpers. Remaining callback time is rollback
fraction times dt, advanced with the post-impulse velocities. The wrapper refreshes
wheel caches and sends the original strength into `CollisionAudio::vehicle_contact`;
sound ownership follows the argument order selected by the normal test.

## Limits and verification

Weapon interactions, damage, destroyed-car coefficients, network events and original
effect dispatch are outside this undamaged/no-weapons reconstruction. The containing
original functions remain partial; RE markers do not imply original class-name
recovery or full implementation. Parent race integration must preserve its bounded
pair rechecking and wall phases; one isolated pair call does not recreate that loop.

`race_collision_test.cpp` exercises original rectangle orientation, distance/height
rejection, positive-time validation, offset rear collision and momentum transfer,
bounded initial-overlap fallback, heading wrap, angular contacts and deterministic
repeatability. The direct strict C++20 build and diagnostic fixture passed. These
synthetic tests validate the recovered subset and do not prove runtime equivalence.

The integrated CMake `race_collision` test also passed. Native and headless
competition checks completed a real original-resource single-lap race with six
vehicles, including actual pair resolution and results-page/lifetime transitions.
Those integration observations are reconstruction checks, not original runtime
comparison evidence.
