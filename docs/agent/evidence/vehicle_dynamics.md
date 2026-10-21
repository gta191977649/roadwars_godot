# Vehicle dynamics reconstruction

Program: RoadWars. Executable SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Evidence date: 2026-10-22. Original C++ class and source-path ownership remain
unresolved; the portable representation lives in `game_src/unresolved`.

## Scope and confidence

The test profile selects the evidenced stock Animal vehicle, Street/Poor tires,
an undamaged human driver, one vehicle, automatic transmission and no upgrades. It implements
the original reachable numerical force expressions and collision/contact helpers.
It does not claim a complete reconstruction of the vehicle class, race dispatcher,
damage, audiovisual effects, controller devices, upgrade selection or networking.
Full dispatcher/partial helpers retain RE-STUB markers. The new reset input returns
to the test spawn; it is explicitly a host test control, not the original lap-based
respawn. The latter still traces/asserts at 004185fe when required.

Static formula confidence is high. Tests exercise the reconstructed code with the
original resources; they do not provide differential runtime equivalence against
the original executable, or justify marking whole classes verified.

## Constructor and geometry

0040d4cb reads METRICS and GEARINFO, converts pounds by 0.453599989414,
foot-pounds by 1.35575044155 and square feet by 0.0929030403495.
0040ee2c confirms Chassis, 1024Body, Tire1..4 and Tire1Ground..4 names.
004d414f/004d4058 computes bounds over the selected object's own raw vertices.
Length is the larger X/Z extent, width the smaller, height the Y extent.
00412834 sets wheel radius to half its Y extent. The first portable profile
loads the original Street tire mesh and Poor friction configuration.

The constructor derives:

- gravity = -mass * 9.80000019073; quarter gravity = gravity / 4;
- inverse mass = 1 / mass;
- drag factor = 0.5 * Cd * frontal area * 1.20000004768;
- partial brake factor = mass * 20000;
- yaw inertia = (width squared + length squared) * ((height / length) * mass);
- pitch/roll inertia = length squared * (0.08333333581686 * mass).

The unusual yaw inertia expression is retained without substituting a standard
rigid-body formula. Wheelbase is the X/Z distance between Tire1 and Tire3.
Fixed tilt radius is the average root-to-Tire1Ground/Tire3Ground distance.

Animal resource probe results: mass 1397.09 kg; length 4.13957, width 1.76836,
height 0.938554, radius 0.311610, wheelbase 2.40844 (original mesh units).

## Force and update order

00480b40 caches RPM, updates material grip (0042b31a), integrates forces
(0042da4b), updates ground orientation (00429a7e), then performs later wheel
suspension and collision phases. The supported profile preserves the force order:
engine, gravity, brake, rolling/lateral friction, drag, steering, displacement cache,
yaw damping, translation, heading/pitch/roll integration.

0042bf6b adds force direction times impulse times inverse mass. Braking compares
the complete old/delta vector magnitudes before adding, and zeros velocity when
the delta is too large. 0042c02f contains two sequential angular braking tests;
positive-rate zero crossing can therefore end with the negative delta itself.
This behavior is preserved and tested.

0042bbf3 computes engine force from torque plus seven integer upgrade values,
rear-end and current gear ratios, rear tire radius, health factor
(health_ratio * 0.7 + 0.3), optional AI difficulty and rear traction clamp.
00413688 converts cached speed to wheel RPM by dividing by wheel circumference
and multiplying by60; 004136b1 multiplies by rear-end and current gear ratio,
then adds abs(throttle)*2000 only when all four wheels are airborne.
00414cf7(0) reads cached speed at vehicle+0x138; the dispatcher refreshes it
after wall responses and before landing responses. Each actual wall hit also
refreshes speed before its own impulse (0042e9e3 explicitly pushes1). RPM, grip rolling
resistance and drag use that cache rather than recomputing the current velocity.
0042cac8 scales engine force by min(1, 1.70000004768 - RPM/6000), halves it for
one airborne driven rear wheel, and zeros it for two.

0042ce7f uses front longitudinal grip for partial braking and front lateral
grip for full braking. It suppresses braking when both front wheels are airborne.
0042d424 applies ordinary quadratic drag and then twice that impulse along the
global down vector. 004dabf4 initializes 0064a1a0 to (0,-1,0); it is not wind.

0042d533 uses tan((90 - abs(steering_degrees)) * 0.0174555554986) * wheelbase
as turn radius. The 90 at 005f5390 is an eight-byte double, whose low four bytes
are zero; reading it as a float would give an incorrect formula. Yaw turning
also uses the original steering influence and throttle-dependent angular term.
0042d88c applies per-grounded-wheel yaw damping with constructor-cached signed
moment arms, rather than an invented steering/yaw drag constant.

The friction helper's complete original special-state timer is outside this
undamaged stock profile; 0042cfa6 therefore retains an RE-STUB marker despite its
reachable ordinary rolling/lateral numerical path being implemented.

The angle conversion constants are the original 0.017455555498600006 and
57.288352966308594, not standard pi/180 replacements. 00517504 is sine;
00517924 is cosine. 0042cc1b's final slope velocity correction pushes impulse
scalar 1.0, not the frame delta. Its airborne gravity tilt calls use fixed radius
and non-braking angular integration. 00429bdf calls only pitch torque; the
portable tilt helper exposes pitch_only to preserve that call.

Grip expressions for the stock profile preserve material matching/mismatch
penalties, doubled longitudinal/lateral coefficients, speed-dependent rolling
resistance and the original positive coefficient floor. Health/power-up, AI
ranking and race-wide modifiers require their original state and are outside
this profile. Original library math uses x87; modern floating-point results are
not asserted bit-identical.

## Validation

`roadwars_vehicle_dynamics_test` passed after a full portable-core CMake build.
It checks constructor-derived inertia, traction clamp, impulse stopping, the
original angular crossing behavior, one/two airborne rear wheel drive scaling,
airborne RPM and downward drag.

`roadwars_vehicle_probe <configured-game-dir>` passed 1200 updates at 1/60
seconds (20 simulated seconds) with original Dark Forest and Animal assets.
Assertions cover finite state, acceleration above 15 original speed units,
heading change and front wheel steering, S switching to original negative throttle,
exact test spawn reset and actual original wall contacts. Reset occurs at frame600.
The test delta is a harness input, not a claim that the original game uses a
fixed 60 Hz timestep.

The earlier combined tilt helper incorrectly applied roll torque to chassis
strikes that originally call pitch only. A fresh probe exposed an unstable
trajectory; the helper now receives pitch_only=true and the fresh full build
and 20-second probe pass. No tuning constants were introduced to mask it.

See `vehicle_dynamics_ghidra.json`, `vehicle_force_helpers_ghidra.json`,
`vehicle_cache_and_axes_ghidra.json`, `contact_orientation.md`, and
`wall_response.md` for retained evidence.

The later RPM conversion correction, automatic gearing, motion-dependent keyboard
reverse and updated acceleration measurements are documented in `transmission.md`.
