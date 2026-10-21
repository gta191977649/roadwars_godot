# Original chase camera and explicit mouse orbit extension

Binary: Kopio RoadWars SHA-256 `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`. Read-only Ghidra evidence is retained in `original_camera_ghidra.json`. Original source filenames and class ownership remain unresolved; portable implementation lives in `game_src/unresolved/original_camera.*`.

## Recovered active camera behavior

`004083b0` constructs mode 4; `00408501` selects mode 4 and marks its position acquired. `00408e09` dispatches mode 4 to `004094fb(dt,0)` and mode 3 to `004094fb(dt,1)`, then `0040ab6a` obstruction. When obstruction supplies an override, the dispatcher calls the chase routine again with the same delta. This second smoothing step is preserved.

`004094fb` uses distance 6.5, extends at 2 units/second and shortens at 4. Positive engine command adds a distance offset approaching +1 at 1 unit/second; braking approaches -1, otherwise zero. The active free-driving profile has no countdown. Nitrous mode has distance 7, height +0.5 (+0.2 in Badlands), pitch -7.5. Nitrous activation is exposed as explicit camera input and is not fabricated by the current profile.

Mode 4 follows actual velocity at cached speed >=2 and vehicle forward below 2; mode 3 always follows forward. Heading uses the original full-vector angle against +Z, signed by X; this intentionally differs from flattened atan2. Desired slope pitch is zero for absolute unnormalized direction Y below the double constant 0.05000000074505806; otherwise it uses the original planar/full-vector angle and signs. Heading interpolation is `004603f3` at delta*2.5 (mode 3: delta*7), pitch at delta*2. Its wrap and overshoot behavior is reproduced rather than replaced by a generic shortest-angle library function. Look-back adds 180 heading, inverts directional Y and output pitch, and resets the angle on its transition.

Eye XZ equals vehicle root XZ minus `(distance + offset) * smoothed_direction.XZ`. Height approaches `(min(distance/desired_distance,1)*0.60000002384 - distance*direction.Y*1.2f + nitrous_height)` at delta*4. Eye Y adds vehicle root Y, the **raw direct 1024Body local Y** obtained through `00428a8f(body,0,1)`, and constructor field +596c. `0040d4cb` reads this field from `CAMERAINFO.HOODZ`; HOODY is a separate field and must not be substituted. Animal uses body Y 0.584002 and HOODZ 0.7: settled level-road eye height is about 1.884 above the root. Summing parent Chassis offsets into this field changes original behavior.

Projection remains the previously recovered 50 degree vertical FOV, near distance 1 and region-dependent far plane. RacePresentation now accepts the resulting portable CameraFrame; view-distance regions and fog camera height sample the actual final eye, including mouse orbit.

## Obstruction

`0040ab6a` extends the camera/body XZ sight line by one forward unit on each end, queries original track wall broadphase using its 3D length, and shifts wall endpoints by their raw normal XZ. It chooses the closest intersection to the body end, repeats up to 51 passes, and sets an obstruction distance clamped to vehicle length*0.60000002384. Chase then targets override-1 with minimum 0.1. Original final ground clearance is camera Y >= ground Y+0.5. The original second chase evaluation can overwrite this clamp; its ordering is retained.

The portable track query already has separately documented coverage limits. This is the active acquired-position chase path, not a claim that intro/countdown interpolation, mode 1, cockpit mode 2, target-object modes, replay cameras, or all camera control dispatch have been recovered. These partial functions retain RE-STUB mappings. `004603f3` alone has a complete RE mapping and contract tests.

## Explicit new mouse feature

Mouse orbit is an additional host feature, separate from OriginalChaseCamera. It has three explicit states: `original`, `manual_hold`, and `returning`. Its eye is represented by world-oriented heading, elevation and radius around the current body pivot. The displayed orientation is derived from that spherical eye plus an explicit composition offset captured from the displayed baseline view. The recovered chase deliberately looks above the body pivot, so retaining this offset keeps the vehicle at the same screen location without an entry/interruption snap; the geometric orbit center remains the actual body pivot.

Manual hold retains that world-oriented sphere and composition while vehicle translation carries the pivot; vehicle turns and chase smoothing do not rotate the held view. No mouse input retains the view for three seconds. Returning interpolates shortest wrapped spherical heading, elevation and radius toward the live chase eye and blends the composition offset toward the live baseline. It never linearly interpolates Cartesian eye positions along a chord through the vehicle. The previous independent Cartesian-eye and heading/pitch blend could shrink the radius and move the vehicle off its stable screen position, especially when interrupted halfway through a return. New mouse input now changes the displayed spherical state directly and restarts the idle period. Once spherical position and composition differ by less than 0.001, original mode copies the recovered frame exactly. The optional orbit uses ordinary geometric degree conversion, while the original chase formulas retain their separately evidenced conversion constants.

Godot mouse-motion events supply screen-relative deltas to the portable core. Active driving captures the mouse to prevent screen-edge clipping, releases it on window/application focus loss, clears pending deltas across focus transitions, suppresses the first capture transition motion, and restores the prior mouse mode at teardown or diagnostic-mode transition. Headless and preview runs do not capture. Defaults remain 0.15 degrees per pixel, orbit elevation bounded to -70..70 degrees, three-second idle delay and exponential return rate 3/second. These are new feature settings, not recovered original constants.

The extension rotates the camera offset around the original body pivot and retains original FOV. Original obstruction applies to the baseline chase; the new orbit does not yet sweep its rotated eye against walls. This limitation is explicit rather than represented as original collision behavior. Camera yaw/position and a deterministic mouse-input method are exposed by GameHost for smoke tests. Diagnostic triangle mode remains independent.

## Validation

`roadwars_original_camera_test` builds and passes in the independent core configuration. Tests use a serialized body-mesh/config fixture to verify HOODZ/body height, original 6.5 distance, throttle/brake offsets, full camera basis/FOV, wrapped angle interpolation, wall normal offset, mouse movement, three-second hold, gradual return to the exact original pose, and rejected negative delta. Godot integration requires the root integration build and runtime smoke checks; this document does not claim that runtime validation itself has passed.

Native integration smoke command: python scripts/smoke_race.py --camera (or add --headless). The script checks three-second hold, gradual exact return, world-heading hold while the car turns, uninterrupted transition from returning to manual control, restarted idle delay, real-eye view-distance submission, finite position and RID teardown. Runtime result is reported by the root integration run.


The 2026-10-22 orbit regression test additionally checks fixed projected body height during a full manual orbit, constant eye-to-pivot radius across a 180-degree return, continuous interruption at its halfway point, translated body pivots, finite extreme pitch input and wrapped yaw. The focused `roadwars_original_camera_test` build and execution passed after this change. The root integration run owns complete core/Godot rebuilds and native camera smoke checks.
