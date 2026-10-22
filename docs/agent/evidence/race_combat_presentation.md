# Race combat presentation ownership

Reference SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Original class and source module names remain unresolved.

`RacePresentation` is a portable rendering adapter. It consumes recovered combat
poses and meshes without advancing timers, randomness, collisions or damage.
Scene barrel poses match the original world child-index paths retained by
`RaceTurrets`. Weapon and ammunition instances have separate renderer handles;
their lifetime remains owned by the portable combat objects. Hidden or retired
pool slots are hidden, including after reset. All handles release before meshes,
materials and textures. Godot only adapts these submissions to RenderingServer.

## Paint selection

Confirmed: `0041c3d9` draws `rand()%6+1`, appends the decimal result to
`Paint Job `, and calls `00411ecc(0,name)`. The latter rebuilds the existing car
body and equipment; its body geometry comes from `Cars/<car>/Body/SpatialObject`
while the primary texture search path is `Cars/<car>/Paint/<paint-name>/`.
The body and shared texture paths are additional original search roots. The
portable high-resolution presentation selects the actual paint texture directory
for the same body MSH, with body/shared fallback. This restores selected paint
textures; it does not claim the whole garage transaction or original low-resolution
quality-selection path is reconstructed.

Retained decompilation: `race_paint_ghidra.json`. Installed archive names confirm
the six `Cars/<car>/Paint/Paint Job N` texture directories. No installed binaries
or extracted resource bytes are included here.

## Oil and particle presentation

Confirmed by `004f8488`, `004f8594`, `004f82e0`, `004f878f` (retained in
`oil_presentation_ghidra.json`): oil uses Textures/xxParticleFX.bmp, black opaque
vertex color, the lower particle atlas, SRCALPHA/INVSRCALPHA blending, disabled
lighting and depth writes, and world-space triangle lists. Every successfully
appended triangle consumes one rand()%4 atlas draw in simulation. The renderer
has a separate147-vertex limit (49 triangles per patch), while collision storage
accepts50. Presentation must not consume randomness or make these limits equal.

Generic and smoke particle pools submit their recovered slot positions, colors,
atlas frames and sizes to native particles without native motion. Smoke Screen
submits its actual asymmetric expanding triangle per slot. All these billboards
face camera yaw only. Flame uses size1 and the lower atlas with16ParticleFX.bmp
and additive blending; atlas selection is therefore separate from blend mode.
The material queue contracts are retained in weapon_trails evidence.

Rocket/Smoke initial raw atlas frames4..9 leave original slot UV uninitialized.
The portable pool retains raw frame and UV validity, advances life normally, and
preserves any previously defined UV. Presentation omits only undefined initial
slots and reports their count explicitly in the debug status; it does not invent
an atlas mapping or count these visuals as verified.

## Limits

Mesh submission is not proof of pixel equivalence. Native beam, jammer, oil,
particle and combat audio presentation require their respective recovered
resource contracts and must be tracked separately from projectile mesh poses.

## Vehicle damage and additional scene effects

Vehicle-owned VehicleDamageVisuals supplies actual mutated body positions and
paint pixels. Presentation keeps renderer instance identity/interpolation while
replacing changed surfaces. Mutable paint textures are keyed by the canonical
original path: two identical car/paint selections share damage, matching the
original global texture manager (004ea922/004eca53 and00420935). Geometry,
mark counters and deformation limits remain per vehicle. Revision counters avoid
uploads on unchanged frames. Normal, UV, index and vertex-color data stay as
recovered; simulation owns mark/deformation limits and random draws. Native
RenderingServer handles release old mesh bases and update the shared texture. Body LOD selection remains the existing high-resolution diagnostic path.

Scene Electron traps, snow, chimney triangles, muzzle flashes, missile flares and
Arachnoid legs read their respective recovered simulation owners. Chimneys submit
the actual asymmetric growing triangles in native list order. Presentation does
not advance particles or consume random draws. Source contracts are in the
corresponding evidence files; resource submission is not pixel equivalence.

The visible vehicle body now uses the same004d3ee3 summed-Euler node poses as
weapon attachments, including00429a7e spring pitch/roll on1024Body. Each
participant retains its own hierarchy and actual simulation angles. Serialized
dummy basis is metadata, and no matrix-composed guessed attachment rotation is
introduced. Wheel poses retain their separately recovered spin/steering path.

Particle render packets preserve each emitter's native draw_order rather than
cyclic slot index. This matters for alpha-mixed Rocket/Smoke pools and keeps
specialized rear blast, impact, tire, health and chimney submissions consistent
with their recovered list ordering. Undefined UV validation still uses the
original source slot before packing, and inactive packet capacity is cleared.
No render submission consumes simulation randomness.
