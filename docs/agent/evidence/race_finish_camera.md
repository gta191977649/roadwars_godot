# Finish camera and health source

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only decompilation/listings are retained in `race_finish_camera_ghidra.json`.
Original class and source names remain unresolved. No original binary was run.

On selected lap completion, `00480b40` calls `004084ab(0)`, selecting race
camera-controller mode5 and the selected car. `00408e09` invokes track routine
`004bb772(selected mesh,0)`. Its assembly corrects the decompiler's missing
stack argument: it compares selected-mesh world position against stored track
camera wrappers, using squared XZ distance, initial999999, strict improvement,
and index0 on ties/outside the initial cap.

`004bb8eb` loads sequential exact Camera1 through Camera50 nodes and stops on
the first missing name. It copies each actual world position to a standalone
wrapper. The chosen wrapper is camera+cc; the tracked vehicle is+d0. Mode7
`004e401b` uses their full3D position difference. Eye is the chosen anchor
because `004bb772` sets78 andb0 to0. Pitch is negative atan of
`(differenceY+7c)/sqrt(differenceX^2+differenceZ^2+b4)`. Constructor `004e2cc7`
initializes7c andb4 to1; the active stock chase path does not replace them.
Heading uses acos of the normalized full3D difference dotted with+Z, native
57.288352966308594 degrees/radian, +180, the negativeX reflection and one wrap.
This is deliberately not replaced by a conventional horizontal look-at.
The native camera update occurs before weather/global effects. No RNG is consumed.

The portable `OriginalTrackCamera` is an unresolved adapter implementing that
stock finish branch. Focused tests cover parent translation, sequential-name
gaps, nearest selection, strict ties, and coincident target normalization.
The full race regression includes its actual pose in cadence comparisons.

Health emitter source is independently confirmed by assembly `00481147`:
vehicle+44cc world point. Initializer `0040ee2c` stores exact HoodPoint there;
RearWeaponMount is the separate44c4 nitrous source. The portable race loads and
transforms the real HoodPoint for every participant. Cockpit-only camera modes
0/2 apply additional source offsets in the native function and remain outside
the diagnostic stock chase/finish profile.
