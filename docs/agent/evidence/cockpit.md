# Cockpit presentation and widescreen extension

Reference: RoadWars x86 PE, base00400000, SHA256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only static Ghidra results are retained in cockpit_ghidra.json. Original
runtime/pixel equivalence has not been tested. C++ ownership remains unresolved;
CockpitFrontend is a portable reconstruction owner.

## Original loading and layout

String reference006138d4 at0042f7f7 identifies initializer0042f549. It loads
Cars/<car>/Cockpit/Data.ini, eight16CockpitN.bmp tiles,16Steer.bmp and
16TackOMeterNeedle.bmp. Keys are WHEEL.SIZE/CENTERX/CENTERY;
TACKOMETER.MAXRPM/BASEROTATION/ROTATIONDEGREES/X/Y/PIVOTOFFSETX/PIVOTOFFSETY;
SPEEDOMETER.MAXMPH/BASEROTATION/ROTATIONDEGREES/X/Y;
MAP.MAPOFFSETX/MAPOFFSETY; WEAPONDISPLAY.WEAPON{1,2,3}{X,Y,AMMOX,AMMOY}.
The track map is Tracks/<track>/Map/16CockpitMap.bmp. The original spelling
TACKOMETER is preserved.

Destination rectangles are x,y,width,height in640x480 logical coordinates.
Assembly receivers at0042f812/0042f858 corroborate that SetRect applies to the
just-loaded sprite; decompiler calls alone can misassociate the next temporary.

| Tile | Destination | Source |
| --- | --- | --- |
|1|0,224,256,256|0,0,256,256|
|2|256,224,256,256|0,0,256,256|
|3|512,224,128,128|0,0,128,128|
|4|0,0,256,224|0,33,256,223|
|5|256,0,256,224|0,33,256,223|
|6|512,0,128,95|0,33,128,95|
|7|512,352,128,128|0,0,128,128|
|8|512,95,128,223|0,0,128,223|

Tile8 source height exceeds the installed bitmap height. A read-only audit of all
twelve cars found this tile128x128 and entirely black; original1555 transparency
contributes no visible pixels. No detail is invented to fill it. Tile4/5 source
height223 to destination224 is retained.

## Animation and readouts

Draw004301f5 submits eight tiles, track/weapon text, tachometer, speedometer,
map when vehicle+4ab0 is nonzero, then steering. Tach angle is
`RPM/MAXRPM*ROTATIONDEGREES+BASEROTATION`. Speed angle is
`speed*2.236997365951538/MAXMPH*ROTATIONDEGREES+BASEROTATION`.
Both reuse16TackOMeterNeedle.bmp and TACKOMETER pivot offsets. No synthetic
digital speed or gear field is introduced.

Steering angle is steering-node heading divided by vehicle MAXWHEELANGLE times
75degrees (005f5384 bytes00009642). Destination uses SIZE and integer
`CENTER-SIZE/2`; odd sizes retain a half-pixel geometric-center offset, with
configured center as rotation pivot. Common sprite004e2490 establishes the
source-rectangle and destination contract already used by portable HUD.

Font10Imp uses136-byte alphabet00613a6c, spacing1, colorff5ab6ff.
Track text uses tile6 destination-left512 and tile4 destination-top+17.
Weapon names use vertical centering; ammo uses both-axis centering.
Weapon initializer004cf29a copies full name+110 to+214, uppercases, then
resolves Localization/Garage.txt. These are full localized names, not short
labels. The current English frontend uses source names.

## Explicit modern behavior and validation

Cockpit parts/text set preserve_aspect on HudQuad. Adapter owns isotropic
viewport transform; portable code owns640x480 layout and angular animation.
Widescreen regions use per-row colors from outermost static cockpit columns.
One-column extension textures cannot duplicate instruments or steering wheels;
transparent rows stay transparent. Extension is new presentation behavior.

Required fields/assets are validated and each supplied participant is preloaded.
Draw reads owned textures only. Invalid index, nonfinite animation, unreasonable
magnitude or malformed resources fail explicitly. The host owns target selection,
snapshot interpolation, camera cuts, visibility and texture-owner lifetime.

Independent C++20 Wall/Wextra compile passed. Installed-asset probe audited all
twelve tile8 images. The root task owns full builds, contract tests, native
captures and integration acceptance; results are recorded after execution.


## Host integration and modern defaults

C/F1 are physical-key aliases for near/far/hood/cockpit cycling, with auto-repeat
ignored. Menus and debug text input consume keys first. Near chase stays the
current original path; far adds2.6 metres and0.5 height with obstruction/ground
checks. Hood uses the authored HOOD vector as a modern interpretation; cockpit
uses recovered HoodPoint. Both first-person views use near0.1 and hide only the
inspected vehicle's body/wheels/attached mounts, preserving simulation visibility.
Released projectiles, other racers and world effects remain visible. Four-view
cycling and hood/far behavior are not original-function completion claims.

Each fixed tick saves pre/post VehicleState samples. Presentation interpolates
steering fraction, RPM and speed with the renderer's exact alpha, creates cockpit
quads without stepping controllers, and reads gear/ammo as discrete state. Reset
and teleport collapse only the affected instrument history; camera cuts never
reset world history. Inspect switches immediately even on zero-tick frames.
Pause retains the snapshot interval; Restart Race retains the mode. Main-menu
return restores near chase. All participant cockpit resources are preloaded;
draw and target selection perform no resource reads or texture allocation.

HudQuad.preserve_aspect requests isotropic reference-space rendering. Godot
converts those quads to pixels using min(width/640,height/480), horizontal center
and bottom alignment. Widescreen extension quads cover the extra width. Ordinary
frontend/HUD quads retain their existing transform. Circle rotation occurs in
reference space before uniform pixel scaling. Static extension columns alpha
composite overlapping tiles so a transparent tile does not erase prior interior.

Diagnostic state exposes mode, target, alpha, previous/current/presented RPM,
steering fraction, actual submitted aspect-quad pixel vertices, near plane and
camera-hidden instance count. These fields are test infrastructure, not gameplay
or persisted configuration.


## Executed acceptance (2026-10-22)

Portable core and Godot extension builds passed. Core CTest67/67 and native
frontend-video-audio/debug-overlay CTest2/2 passed. The new synthetic cockpit
fixture covers geometry, steering pivots/75-degree rotation, readouts, malformed
inputs, repeated-render determinism and texture ownership. Optional installed
asset tests passed for all12 cockpit configurations and all12 camera anchors.

Native Vulkan cockpit smoke passed for Animal and Avenging Angel: allfour views,
C and F1 edges/repeat rejection, actual submitted square-wheel geometry at
960x720,1280x720,1280x800,1680x720,1920x540, repeated presentation without physics
mutation, actual Inspect AI/player selection, shared RPM interpolation fraction,
variable cadence plus350/900ms catch-up, pause/resume and original-menu restart.
The final cockpit warm-up reaches live driving before captures. Headless cockpit
smoke also passed. All12 cars were separately captured at allfive dimensions;
4:3 and32:9 contact sheets were visually inspected, along with full-resolution
Animal and Avenging Angel views. Ignored build artifacts retain captures/logs.
Side extensions deliberately show stretched static edge colors; they are not
newly painted or geometrically reconstructed interior panels.

The existing native camera/orbit smoke and native timing smoke passed. Portable
frame-independence retained bit-identical1200-tick traces across11 rates, jitter
and350/900ms stalls; native timing retained identical300-tick vehicle/effect
state. Existing native and headless complete competition, headless Single/Quick
menu launch/return, and frontend preview/lifecycle smoke passed. Progress,
portable boundary and whitespace checks passed. One regression initially caught
Inspect cuts delayed until the next tick; immediate view-aware cuts fixed it
without resetting world histories. Native game rendering, not the original
executable, produced these images; original pixel/audio/runtime equivalence,
subjective appearance on every physical monitor and additional cockpit damage
visual effects beyond the recovered tiles/readouts are not claimed.
