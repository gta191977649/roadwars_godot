# Original stock-wheel attachment and animation

Program RoadWars; Kopio executable SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Date 2026-10-22. Read-only Ghidra MCP and original resource inspection; no original
executable execution. Original class/source ownership remains unresolved.

## Attachment error and recovered behavior

`0040ee2c` locates Tire1..4 and Tire1Ground..4 under the original vehicle hierarchy.
`00412834` loads the selected tire model separately, applies two vertex reflections
for right-side indices 1 and 3, and attaches each tire model under its TireN anchor.
The attachment function `004d2ea1` explicitly resets the attached child's local
translation and heading/pitch/roll to zero. Therefore its serialized root position
must not be added to the anchor position.

Animal Street SpatialObject.msh root Cylinder02 has serialized position
(0.00177338,0.36770648,1.20203376). Its own raw vertices already describe a wheel
with axle along X: bounds X [-0.137595,0.110138], Y [-0.311610,0.311610],
Z [-0.327646,0.327646]. Adding that root position again raises and shifts every
wheel incorrectly; an invented 90-degree wheel rotation does not repair it.

`004d2509` negates every lit vertex X; `004d264d` negates Z; both recompute mesh
normals. Combined they are equivalent to a local 180-degree Y rotation with
positive determinant. Apply the same X/Z sign changes to normals when expressing
the operation as a transform. UV and triangle indexing are unchanged. The original
loader applies this to right-side wheels and their LOD variants.

`00412834` reads [SCALE] SCALEX from
Cars/<vehicle>/Tires/<tire>/<quality>/Data.ini, through the PSE-preferring loader.
Poor/Good/Best Animal Street X scales are 1.0/1.2/1.4. The stock profile uses Poor.
It scales X only; tire radius/circumference uses unscaled Y extent. The original
radius is half the Y extent; circumference multiplies by float 005f5214=6.283185482.

Type-4 dummy serialization contains three vectors, but `004d9aca` does not assign
them as a complete runtime rotation matrix. It stores the position and computes
separate heading metadata at +0x1f0 from the normalized second vector. Applying all
raw vectors as a transform would introduce unsupported orientation/reflection.

## Steering, spin and suspension

Front Tire1/Tire2 node heading is the original steering angle (see `00424ae8`).
Rear Tire3/Tire4 remains zero in the stock profile. The wheel node pitch stores
spin; the original transform uses Rx(-pitch), combined with heading and the parent
vehicle transform. `00428958` clamps chassis pitch only for a null/root object;
non-null wheel objects retain unbounded pitch. Do not clamp wheel spin to +/-60.

`00428cb6` requires nonzero cached speed and an enabled/resolved body LOD, excluding
a special body mesh. It obtains current velocity/forward angle through `0042beed`.
When the angle lies strictly between 45 and 135 degrees, projected wheel speed is
zero. Otherwise:

```text
projected_speed = abs(cos(angle * 0.017455555498600006)) * cached_speed
degrees_per_second = (projected_speed / front_wheel_circumference) * 360
step = degrees_per_second * dt * (1 - brake)
```

Only grounded wheels (air timer zero) spin. Forward angles below 90 subtract step
from wheel pitch; angles at least 90 add it. The same front-wheel circumference is
used for all four wheels. Full braking stops spin. The helper accepts caller-owned
angle and LOD enable state and therefore retains a partial RE-STUB mapping.

`0042a11c` adjusts both TireNGround and TireN local Y by the same suspension delta.
Rendering must use the actual current ground-local offset already maintained by
VehicleDynamics, added to the original axle anchor. `00429ff5` likewise restores
both anchor sets by the same ground baseline delta. Do not introduce a second
visual suspension simulation.

## Portable implementation and validation

wheel_pose.* provides stock scale loading, original wheel spin integration and
local wheel transform composition, including right-side mirror and front steering.
These are explicitly partial subsets of `00412834` and `00428cb6`; purchase,
upgrade/LOD switching, special body guards and complete hierarchy ownership are
not recovered. Caller resets the loaded tire root pose and applies its vehicle
world transform. Unknown original paths remain trace/assert elsewhere.

Direct GNU C++20 contract test passed: forward/reverse spin, sideways no-spin,
full-brake stop, airborne exclusion, body-disable guard, invalid circumference,
original anchor preservation, right-side reflection and front-only steering.
Parent integration must register source/test targets and run fresh render/drive
acceptance. This is not original-runtime equivalence verification.

Retained raw Ghidra records: wheel_render_pose_ghidra.json. Original models and
configuration files remain outside version control.
