# Original AI controller subset

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Retained static decompilation and memory reads: [race_ai_ghidra.json](race_ai_ghidra.json).
The original executable was not run. The existing Ghidra RoadWars program supplies
the hash-bound addresses; no adjacent project's code or findings were used.

## Ownership and confirmed fields

`00480b40` dispatches `0041aa67` on participant vehicle objects. Its `this`
references the same vehicle fields initialized by `0040d4cb`, including wheel
nodes at 44a0/44a4, engine command at 4fb4, brake at 4fb8 and maximum wheel angle
at 4f80. This establishes vehicle ownership with high confidence, but does not
establish an original class name, vtable identity or C++ source filename.
`RaceAi` is an explicit portable representation of that vehicle-owned state, not
an assertion that the original game had a class named RaceAi. It lives under
unresolved rather than an invented source hierarchy.

| Field / function | Confirmed observation |
| --- | --- |
| 4bac, 4bb0[8] | Behavior integer and speed biases. Constructor values are 2,0,-2,-4,2,2,.5,0; `00484189` sets initial behavior 1. |
| `00418f73` | Mode 1 baseline, 2 health deficit >=25, 3 damaged/rear contact, 4 rear only, 5 front+rear, 6 front only. Descriptions are branch predicates, not recovered enum names. |
| `00419597` | Four triangular sectors: rear176/184 degrees length20, front-4/4 length20, left-140/-60 length6, right60/140 length6. Priority rear/front/left/right; maximum strength is 1-distance/20. |
| `0041906a` | Timed lateral offsets, obstacle response, mode-specific lane behavior, and offset convergence. |
| `004182ee` | Sequential route samples -1,0,1,2,3 with shared branch latch and mutable lateral clamp. Recovery additionally samples -2. Implemented by RaceRoute. |
| `0041bcb7` | Shortest signed heading delta, corner threshold3 degrees, speed10000 below threshold. |
| `0047c820` | All AI distance calls use XZ only. The separate `0047c742` is 3D distance; route-height values must not influence lookahead interpolation or sector strength. |
| `0042bd5c` | Corner speed sqrt(grip*gravity*distance/(angle*radians)). |
| `0042bdc7` | Braking estimate terminal + sqrt(terminal^2-2*grip*gravity*distance); negative-root result falls back to terminal. Unconventional subtraction is preserved. |
| 5028 / `0042b31a` | Grip is average four longitudinal coefficients, not mass or brake force. Caller supplies prior physics-step value. |
| `0041aa67` | Two-stage lookahead target, route speed hints (positive maximum, negative minimum), speed-based direct throttle/brake, obstruction reaction, steering curve and smoothing. |
| 4ab4/4ab8 / `0042e8f0` | Actual forward-throttle wall contacts accumulate recovery time; >1.5s without wall contact clears it. |
| 4abc / `0041aa67` | Failed recovery cycles add7.5; accumulation decays; >=15 triggers original route-based relocation and one-node advance. |

`00480b40` rotates a participant index. Only that vehicle refreshes obstacle
sectors/mode during `00418edd`; all vehicles update lateral convergence and
driving control. Caller supplies this refresh flag to preserve scheduling.

## Numerical and control details

Memory reads establish float constants gravity9.800000190734863,
radians0.017455555498600006, degrees57.288352966308594, speed conversion
.4470277726650238, initial acceleration threshold.25 seconds and steering blend
min(7*dt,1). Thresholds at005f5288,005f5298,005f52b8 and005f52c8 are **doubles**
5,2,3,180; interpreting them as floats incorrectly gives zero.

AI outputs direct engine/brake/steering values, bypassing keyboard brake ramp
and discrete steering. The original small-angle steering curve contains offsets:
error*3+2 below2 degrees, error*5+5 below5, otherwise full wheel lock with sign.
Steering follows `004603f3` shorter-arc interpolation, then subtracts360 if >180.

Recovery uses backward route samples and a101.8-second tagged timer, alternating
reverse throttle and .99 brake near100.6 before clearing below100. Low-speed and
wrong-way gates follow afterward. Route-based relocation is observed recovery,
not a test-only teleport to fabricate lap completion. Tests must not increase
progress independently of normal race progression.

## Verification and remaining limits

Standalone GNU C++20 warning-enabled build passed. `race_ai_test` passed helper
reference values, original mode branches, acceleration threshold, direct steering,
wall recovery/clear, finish commands and invalid input checks. External installation
checks sampled every route node in Dark Forest and RWA Speedway and produced
finite control/target values. A height-invariance regression covers lookahead
interpolation after static review identified the XZ/3D helper distinction.
This validates input/command safety, not integrated
race completion or original-executable behavioral equivalence.

`0041a1cd` and descendants handle weapons/attack behavior. They are retained
as evidence and intentionally excluded from this no-weapons profile.
Damage-related mode selection is available as a pure helper; live racing uses
undamaged health inputs. Destroyed-obstacle lateral bias4acc, dynamic weapon/
equipment branches, native caches and exact original runtime floating-point
equivalence remain partial. Source markers do not promote containing functions
to implemented/verified.
