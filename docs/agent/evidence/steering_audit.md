# Steering sign and presentation handedness audit

Date: 2026-10-22. Read-only Ghidra program: RoadWars.
Binary SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Retained decompilation: `steering_audit_ghidra.json`.

## Confirmed original sign

`004831c3` reads Controllers/Keyboard/Data.ini, selects INPUTS and stores RIGHT
at controller offset 0x3e8 (1000), LEFT at 0x3ec. The installed encoded PSE,
which the original parser prefers, assigns DirectInput scan codes 205 and 203.
`00424d3e` tests the field at 0x3e8 and writes steering target 4fc0=+1.0;
the 0x3ec branch writes -1.0. Therefore positive input means right in original
simulation coordinates. Current GameHost D/Right=+1 and A/Left=-1 matches this.

The original yaw primitive `004fdde7` writes row 0=(cos(h),0,-sin(h)) and
row 2=(sin(h),0,cos(h)). `004d3dbd` composes that yaw with pitch/roll. Thus
positive heading turns original +Z forward toward original +X right.
The recovered `0042d533` turn helper selects the original right vector for
positive wheel angle, left for non-positive angle. Its sign logic is retained.

## Presentation defect

The initial RacePresentation chase camera generated a standard right-handed
look-at basis directly in original world coordinates. At heading zero it looked
toward +Z with screen-right basis -X. Consequently an original right turn toward
+X appeared to turn left on screen. This is a world presentation reflection,
not a reversed original keyboard mapping.

Preserve original input, simulation and collision coordinates. Adapt geometry,
normals, object transforms and camera together at the rendering boundary.
One consistent adapter is reflection S=diag(1,1,-1): positions S*p, mesh-local
vertices S*v, object basis S*R*S. Reflected mesh winding must be adjusted if
back-face culling is enabled.

The existing portable chase-camera pose already has a right-handed basis, so
camera conversion is not the same as object conversion. Reflect eye/target/up
and rebuild a proper right-handed look-at basis, or use columns
(-S*old_right, S*old_up, S*old_back). At heading zero the result has Godot
screen-right +X, screen-up +Y and forward -Z. Merely negating A/D would hide
the symptom while leaving the world and resource details mirrored.

## Validation and limits

`tests/core/steering_test.cpp` checks the original right/left target signs,
the exact first-step rate, return to center, saturation and yaw-forward signs.
Rendering-boundary projection and visual checks are parent integration work.
The chase camera remains an explicitly new test camera; this audit does not
claim recovery of the original camera controller.
