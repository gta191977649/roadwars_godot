# Camera-facing track sprites

Reference: Kopio RoadWars SHA256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra and original-asset observations, 2026-10-22, are retained in
billboard_ghidra.json. Source paths/class ownership remain unresolved.

## Observed behavior

`004dce9b` builds the shared host+0xc29c matrix from camera+0x34 heading plus
180 degrees (`005f50a0`), multiplied by float `005f5ab4` =
0.017455555498600006. Matrix rows are (cos,0,-sin), (0,1,0), (sin,0,cos).
`004d314d` copies that matrix for resolved flag 0x40 and supplies the object's
world translation. It does not use the vehicle heading or camera pitch.
These are upright, yaw-only screen-aligned sprites.

Dark Forest contains 214 flag-0x40 nodes, including trees and flares. Tree planes
are local XY with +Z triangle cross normals. Increasing U runs toward local -X.
After the existing local/world Z reflection, the correct native model columns are
(-cameraRight, worldUp, -cameraBack). Both horizontal axes must flip together to
preserve atlas orientation, face winding and determinant. Local nonzero Z offsets
such as flare geometry must also be retained.

## Correction and limits

The former presentation tied these sprites to the vehicle's yaw, even while the
displayed camera orbited independently. That loop is removed. Material flag 0x40
now selects `game_src/shader/original_surface.gdshader`, where the current render
pass's inverse-view matrix provides the horizontal right axis. The shader replaces
the model-view basis while retaining world translation, per-axis scale, upright
Y, original atlas handedness and transformed normals. Fog depth is calculated
after billboarding. This also follows interpolated cameras without a CPU sprite
rotation loop or camera-heading uniform. Native view-basis use is an integration
adaptation, not a bit-exact reproduction of the original approximate pi arithmetic.

Spark and tire draw shaders use the same GPU camera basis; CPU per-particle-system
billboard-right uniform updates are removed. Original particle motion, lifetime
and atlas coordinates remain portable and unchanged.

Billboard mesh bounds are expanded once at upload to the radial XZ extent, avoiding
frustum rejection of geometry rotated by the GPU outside its serialized flat-plane
AABB. Static track instances are excluded from CPU transform interpolation, while
animated track parts and vehicles remain dynamic. Full original LOD, hierarchy,
scale-sign and render-queue equivalence remain unproven; mappings remain partial.

## Validation

`python scripts/smoke_billboard.py` uses the actual RenderingServer shader and
back-face culling. Its asymmetric red/blue fixture has original sprite winding,
UV handedness and nonzero local Z. Eight yaw views and three pitches assert that
both halves stay visible, red remains screen-left and projected widths do not
collapse edge-on. It also checks scene teardown. This fixture validates the native
adaptation; it is not a differential original-executable capture.

See original_camera.md for the separate spherical-orbit pivot correction.

## Executed checks (2026-10-22)

Independent portable build `build/billboard-core` and all 28 CTest cases passed.
The original-asset renderer probe found 214 Dark Forest billboard surfaces and
42 Underground billboard surfaces; vehicle/camera changes issued no CPU transform
updates to those static sprites. Animated surface counts remained 2 and 4.
The actual GPU fixture passed all 24 views. Original-asset preview, particle and
camera smoke passed, including scene restart and release. Final extension objects
were also linked into an isolated validation output while the shared DLL was held
by a running game; that output passed the GPU, camera and particle runtime scripts.
A preliminary headless editor import of the isolated project exited with Windows
0xc0000005 after import; its subsequent direct runtime scripts passed. This is
recorded separately and is not claimed as a passing editor-import check.
Progress update/check, portable boundary check and diff whitespace check passed.

After the running game released the shared DLL, the standard Godot build linked
the final extension into the normal project output successfully.
