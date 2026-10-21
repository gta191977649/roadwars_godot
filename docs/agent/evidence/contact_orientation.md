# Contact orientation and original rotation order

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Full read-only Ghidra decompilations are retained in contact_orientation_ghidra.json.

The chassis-strike impulse magnitude uses `VehicleState.cached_speed`, refreshed by
the dispatcher before landing and wall handling. Current velocity still determines
the incidence angle. This distinction follows `00414cf7(0)`; see
vehicle_cache_and_axes_ghidra.json.

`00429a7e` recomputes wheel world points, samples ground, updates airborne timers,
sets root height, updates chassis pitch then roll, clears angular rates when all
wheels are grounded, then applies independent body pitch and roll springs.
The new adapter accepts the already-computed four world points and samples.
It does not claim recovery of the frame hierarchy or missing-ground respawn.
Ground query input Y is raised by float 005f51d0=3.0 in 004297dd.

`0042988c` increments a wheel timer only when sampled ground is below its world
anchor and their Euclidean distance exceeds float 0.1. It does not reset timers.
When sample zero has Y=-99999 it increments a separate timer. At one second the
original calls respawn selection `004185fe` and clears velocity/angular state.
The portable helper returns unavailable at that branch so its caller must trace
and fail; it does not fabricate a respawn.

SHA-bound text disassembly finds the gameplay timer reset at 0042a708 inside
0042a11c. That later suspension/landing function computes desired wheel-local Y
as current local Y minus (world anchor Y minus ground Y). If it is at least the
lower suspension bound 4734+i*4, the function handles any airborne landing and
clears the timer; above the upper bound 4744+i*4 it also caps wheel position.
Below the lower bound it clamps the wheel without clearing the airborne timer.
Its impact solver is not part of update_contact_orientation. Race initialization
also clears all four timers at 00483aad inside 0048355e. These decompilations are
retained in contact_landing_ghidra.json.

The separate update_wheel_suspension helper implements this wheel-local clamp and
landing branch with constructor bounds at original wheel-local Y +/-0.1. A
landing angle greater than 90 degrees damps velocity by
min(1,1-abs(cos(angle*0.017455555498600006))+0.25), pitch/roll rates by 0.8,
and assigns body spring targets with magnitude 10*min(angle-90,15)/15.
Front wheels use positive pitch, rear negative; left wheels negative roll,
right positive. The original impulse vector assembled in the local array feeds
0041553d haptic feedback rather than the vehicle impulse helper. Audio/haptics
are host concerns. The final 00429bdf chassis front/rear ground-strike check is
outside this helper, so 0042a11c remains a partial RE-STUB.

Ghidra incorrectly inferred void for `00429714`. SHA-bound x86 disassembly at
004297cc-004297d5 loads its accumulated float onto ST0, multiplies by float
005f523c=0.25 and returns it. It averages max(anchorY,groundY) for all four wheels.
The temporary wheel-local Y difference calculated inside its loop is unused.

`004292fe` averages front/rear inclinations for left pair (0,2) and right pair
(1,3), replacing an airborne pair with current pitch. `00429507` averages negated
left/right inclinations for rear pair (2,3) and front pair (0,1), replacing an
airborne pair with current roll. Inclinations use `0047c1b4` clamped cosine
against the XZ projection, acos, and float 005f5324=57.288352966308594. The sign
is reversed when the first point is below the second. Contact normals are not
used in these angle calculations.

`004286e0` moves a body angle at float 005f533c=45 degrees per second; it allows
overshoot and uses strict crossing comparisons. Crossing zero halves the stored
history into a new target, except abs(history)<double 005f5330=1 clears it.
This is not a conventional damped spring and must not be replaced by one.

`00429bdf` tests the front and rear chassis endpoints at half vehicle length
around the body frame world origin. Forward is unit Z rotated first around
negative X by pitch, then positive Y by heading; roll is ignored here. Global
axes are initialized at 004dab4a/004dabd2/004dabb0. `004bb23c` outputs its
penetration flag exactly when sampled ground Y is at least query Y, so merely
finding a triangle is insufficient to trigger a strike. Front then rear strikes
apply ground-normal impulses with magnitude cachedSpeed*abs(cos(angle))*2*mass
divided by float 005f5348=8.5 for front and 005f5128=4 for rear. The same scalar
is passed to fixed-radius angular impulse, with braking false. Each strike also
adds groundY-endpointY to root Y. Endpoints and wheel references remain cached
across both strikes. Decompilation is in chassis_ground_strike_ghidra.json.

`004d3dbd` obtains accumulated heading/pitch/roll via 004d5790/004d57e0/004d5830.
It builds row-vector matrices with 004fdde7 (Y), 004fdd6d (X), 004fdea1 (Z), then
multiplies X(-pitch)*Z(roll)*Y(heading) using 004fd7d2. Degree conversion is the
original float 005f5a9c=0.017455555498600006, not a modern pi/180 replacement.
`original_vehicle_transform` implements the resulting standalone transform for
already-accumulated angles, not the original hierarchy/cache ownership.

The fixture test exercises spring overshoot and small-history termination,
heading direction, pitched ground, height averaging, airborne pair fallback and
explicit unavailable respawn. It is a recovered-formula contract test, not a
comparison against a running original executable.
