# Keyboard reverse, automatic transmission and acceleration audit

Program: RoadWars. Executable SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Evidence date: 2026-10-22. Original class/path ownership remains unresolved.
Retained database exports: `transmission_ghidra.json`.

## Confirmed corrections

`00413688` is incorrectly represented as a void call by the decompiler. Its
assembly at `0041369e` divides cached speed by vehicle+0x4f34 (wheel circumference)
and `004136a4` multiplies by 60. `00412834` sets circumference to wheel radius times
the original float 6.2831854820251465. `004136b1` then multiplies wheel RPM by rear-end
and current gear ratios. The previous reconstruction omitted circumference and
60, severely understating RPM and therefore keeping excessive engine force at
high speed. This is corrected without changing torque, mass or grip constants.
All-airborne RPM still adds abs(throttle)*2000.

`00424d3e`, keyboard instructions `00426b82..00426d1b`, resolve direction against
the angle between actual velocity and vehicle forward. W requests positive
throttle if stationary or angle<90; otherwise it brakes. S requests negative
throttle if stationary or angle>90, and repeatedly shifts down to gear index0;
otherwise it brakes. There is no invented near-zero speed threshold. At90 degrees
both keys brake if moving. W executes before S, so both held while stationary
select reverse. Unrequested throttle/brake values clear each frame.

Braking ramps the persistent brake amount by dt*4 below 0.6000000238418579,
otherwise by dt*0.550000011920929, and clamps to1. Reverse uses the same first
gear ratio with negative engine throttle; it has no extra reverse ratio slot.
The new reverse_engaged field describes the resolved command, not an alleged
original persistent reverse flag.

`00413200` clears the upshift flag and, when automatic transmission is enabled,
upshifts above6000 RPM; otherwise it downshifts below4000 with positive throttle,
or below3000 regardless of throttle. `0041328f` and `004132db` enforce the ratio
table bounds. `00480b40` refreshes RPM before integration and automatically shifts
after collision work using that same cached RPM. The chosen new gear affects
the next frame. Manual gear key bindings, joystick and network controller branches
remain outside this keyboard profile; the complete controller retains RE-STUB.

## Force, grip and clock audit

`0042bbf3` computes (converted torque + seven integer upgrades)*rear-end*gear /
rear tire radius, times the health factor(.7*health+.3), optional AI difficulty,
then clamps to the rear traction force. `0042cac8` applies dt*drive_force*throttle*
min(1,1.70000004768-RPM*0.000166666664882), then inverse mass in `0042bf6b`.
One airborne rear wheel halves force; two suppress it. There is no missing 1/4
factor, clutch interpolation or shift delay in these recovered helpers. The
unusual possibility of a negative force factor at sufficiently high RPM is
preserved, without adding an unsupported lower clamp.

`0042b31a` doubles material longitudinal/lateral grip after modifiers. Street
tires map to kind0 (`00412834` string STREET at0061317c). Matching material adds
twice the tire upgrade friction; mismatched lateral grip>.4 subtracts .05 for
off-road kind2 or .1 otherwise. Stock Poor upgrade friction is0. Rear traction
is average rear longitudinal coefficient*mass*9.8; partial braking uses the front
longitudinal average and full braking the front lateral average. Rolling uses
min(cached_speed/26,1)*material_resistance+.02, with positive floors. Character
traction scalar at+0x43fc multiplies longitudinal/lateral coefficients: `0040d2b0`
loads PRICE_SCALARS/TRACTION from Characters/<character>/Data.ini. The selected
Animal character has1.0, so the existing stock profile is numerically correct.
Race rank, damaged and power-up modifiers are still outside the stock profile.

`0045c29c` reads timeGetTime, divides milliseconds by1000 (float005f50b4), clamps
to.1 seconds. `004a4f57` forwards that delta to `00480b40`, which invokes one force
integration per vehicle for that delta. No multiplication of driving time or
fixed substep loop appears in this path. The host uses this same seconds contract.

## Validation and limitations

The portable core builds. Unit tests independently check 400Nm*3*2/.5m=4800N,
dv=F/m*dt, the6000 RPM .7 force multiplier, negative drive, per-wheel contact
scaling, wheel RPM, grip force aggregates, brake ramps, simultaneous keys and
automatic shift thresholds. Steering and particle tests also pass.

The original Dark Forest/Animal probe completes20 simulated seconds, including
actual reverse, steering, reset,14 wall contacts,4 accepted spark bursts and40 peak tire
particles. A separate fresh W-only probe reports:

| Seconds | Speed (original units/s) | Cached RPM | Forward gear | Wall contacts |
|---:|---:|---:|---:|---:|
|0|0|0|1|0|
|3|24.6275|5102.92|2|0|
|6|24.7749|5155.35|2|1|
|10|29.8775|4573.92|3|2|

Later samples include track turns, collisions and slopes and are not a flat-road
acceleration benchmark. These are reconstructed runtime measurements plus static
formula checks; no original-executable differential driving capture was performed.
They do not establish full vehicle equivalence. S now brakes then reverses, so
the old test assertion that holding S always reduces speed was replaced with
explicit original reverse-command assertions.

Skid metadata follows `00413425`: grounded and (slide or full brake), separately
from the dust emission predicate. Position is wheel contact plus0.10000000149Y;
color comes from material+0x14 and half width from half the smaller raw tire X/Z
extent (`00412834`).
