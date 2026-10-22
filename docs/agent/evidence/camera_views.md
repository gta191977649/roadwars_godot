# Cockpit camera and modern view cycle

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only static decompilation/listings are retained in camera_views_ghidra.json.
No original executable was run. Original class/source ownership remains unresolved.

## Confirmed native behavior

00408e09 selects 00409420 for native camera mode2. The dispatcher also changes
the near distance to0.1 in this mode, compared with1 elsewhere. Its first calls
refer to the selected car's world mesh visibility; host rendering owns that
presentation concern in the reconstruction.

The listing at0040943f identifies the eye receiver as vehicle+44cc. Initializer
0040ee2c binds this field to exact HoodPoint, distinct from RearWeaponMount+44c4
and 1024Body+449c. 004d3ee3(1,1,0) returns its hierarchy-transformed world point.
00409487 obtains root heading via00428b07(0,1). Calls004094a8 and004094c9 use
1024Body as receiver for cumulative roll004d5830(1) and pitch004d57e0(1).
The portable camera retains those rotations and converts the original basis to
the existing right-handed camera contract by reversing its right and forward
columns. The serialized chassis root translation is replaced by VehicleState,
matching current vehicle presentation and the Avenging Angel correction.

0040d4cb reads INCARX into+595c, INCARY into+5964, INCARZ into+5960, then
negates+595c. HOODX goes to+5968, HOODY to+5970, HOODZ to+596c. The resulting
portable vectors are therefore INCAR=(-x,z,y), HOOD=(x,z,y). A bounded static
search for these field immediates in executable00400000..00580000 found no
INCAR readers outside initialization; HOODZ is independently used by chase
height. Consequently the native cockpit uses HoodPoint rather than an invented
INCAR translation. The configuration is retained as metadata.

## Explicit modern presentation additions

CameraView and its four-view cycle are new infrastructure. near_chase copies
the existing CameraFrame unchanged, preserving the acquired original camera
and countdown/finish inputs supplied by its caller. far_chase extends the
near eye/pivot direction by2.6 and raises Y by0.5, then applies the existing
wall-distance predicate and ground clearance. These distances are user-approved
modern defaults, not recovered original constants. hood uses the authored HOOD
vector relative to 1024Body and current vehicle orientation; this interpretation
is also a new view, not original native mode1 (which is a separate chase algorithm).

The camera view function is stateless, consumes no RNG and does not mutate the
vehicle. Input edges, visibility, camera cuts and snapshot timing belong to the
caller. Missing camera configuration/HoodPoint is an explicit error.

## Validation

The focused portable contract verifies six configuration values and original
axis remapping, hierarchy HoodPoint translation while ignoring an exported root
offset, exact near passthrough, four-step cycle, far extension, modern hood eye,
tilted/rolled cockpit eye and basis, unchanged input and rejected invalid mode.
Native rendered validation and full builds are owned by the integration task.
The focused core target build and executable passed. An ignored installed-asset
probe also loaded all twelve character configurations/HoodPoint hierarchies and
successfully computed each of the four views. No rendered camera parity is
implied by that asset-loading probe.
