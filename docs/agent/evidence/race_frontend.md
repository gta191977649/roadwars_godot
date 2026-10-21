# Race frontend reconstruction

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Ghidra MCP program: `RoadWars`, x86 Windows, image base `00400000`.
Queries are retained in race_frontend_ghidra.json and
race_frontend_supplement_ghidra.json. Static analysis does not observe execution.

## Confirmed observations

- `00484189` allocates a 0x24b0-byte frontend object through `00487940` and
  initializes it through `00487fbe` after loading the track and racers.
  Original class name and C++ source path remain unresolved. `RaceFrontend`,
  `HudBitmapFont` and `FrontendMapSettings` are new portable representations.
- `00487940` reads RaceInterface/Options.ini, DATA SPEEDOMETER, TIME,
  LAPUPDATE, TRACKMAP and METERPORTRAITS. The reconstructed subset consumes
  SPEEDOMETER, LAPUPDATE and TRACKMAP; cockpit/portrait mode switching is incomplete.
- `00487fbe` places xxSpeedOMeter at (512,352), reads its dimensions from the BMP,
  places its needle at (546,398), and defines the needle pivot (577,418).
  Digital speed is at (583,425); gear text is at (570,452). It loads character
  16MapDot and 16MapPortrait and RaceInterface/16MapDotBorder. The player border
  is confirmed by the ECX receiver in `0048b094..0048b0a0`.
- `0048b37d` draws cached speed times float `005f53a8` (2.236997365951538),
  truncated toward zero, so the digital readout is MPH, not km/h. Its needle is a
  tachometer: `(RPM / 8000) * 165 + 60` degrees, without an invented clamp.
  Gear labels are the localized REV/1ST..7TH keys; this profile uses the English
  labels. Arbitrary language lookup remains incomplete.
- `0048ace2` subtracts forward Start1 (`004bb389(1,1)`, Y cleared), multiplies
  by track field 0x7bdc, applies the 0/90/-90/180 mapping and adds Start1 map
  coordinates plus 0.5 before __ftol. The assembly at `0048adf5..0048aebc`
  resolves signs absent from decompiler output. Centered dots use integer
  half-width/height. Non-player tint is 100/255; the player is white with a border.
- `004b7b2e..004b7ba9` binds TRACKTO2DMAPSCALE, START1_2DMAPX/Y and cockpit
  offsets to fields 0x7bdc..0x7bec. MAPROTATIONDEGREES is loaded by `00487fbe`.
  Dark Forest data supplies scale 0.0459997, map anchor (67,52), rotation 90.
- `0048be66` draws the 128x128 map at (0,352) with alpha 0.7; portrait slots
  are (4,360+20*rank), 17x17. The trace profile explicitly selects this map view
  and the Animal character, independently of the car model selection.
- `004e14dc`, `004e183d`, `004e1890` scan blank columns/rows in the top-down
  font surface. The first alphabet bytes at `0061a0c8` supply A-Z, apostrophe,
  comma, digits and slash. Spacing is set to 2 by the frontend initializer.
  `004e0a26` emits glyph quads. Numeric HUD text uses the original atlas and
  vertical alpha gradient 0xb2ffffff to 0x66ffffff.
- `004e1a88` derives bitmap rectangles from their true width/height;
  `004e2490` maps 640x480 coordinates to the actual display and uses inverted V
  coordinates over the original bottom-up texture surface. The reconstruction
  retains this in portable UVs, including separate XX alpha and 16 black keying.

## Ownership and supported contract

All loading, bounded bitmap font parsing, logical layout, speed/RPM/gear display,
map projection, character markers, portrait/status selection and quad generation
live in roadwars_core. RacePresentation owns RaceFrontend and its texture lifetime.
Godot receives only opaque texture handles and colored textured quads, draws them
on a RenderingServer canvas and scales the logical frame to the viewport.
The canvas sits below the diagnostic overlay. Teardown clears commands before
destroying textures and detaches/frees both canvas RIDs at renderer destruction.

No original class is invented or promoted. Functions stay analyzing because the
portable requested subset is not the entirety of the original frontend, font and
sprite APIs. Original cockpit, damage/weapon displays, time/countdown/results,
all localized glyphs and legacy DirectDraw branches are not claimed recovered.
Race ordering, lap counts and timing must come from the future race dispatcher.
The core accepts explicit race lap/rank values; trace mode omits them rather than
fabricating progression. No source-only scaffolds are counted as implementations.

## Validation

- Portable race_frontend test: fixed assembly-derived projections for all four
  rotations, negative truncation, font row transitions and malformed atlas rejection.
- Original-asset invocation of roadwars_race_frontend_test with the configured
  installation: nine textures, original font scan, Dark Forest parameters, actual
  quad dot displacement, invalid player index, hide/reload/missing-track cleanup.
- `python scripts/smoke_race.py --frontend`: actual Godot raster comparison finds
  25042 map-region and 24979 meter-region changed pixels when toggled; real vehicle
  motion updates the dot, reset restores (67,404), resize runs, three scene restarts
  keep the same RID count and exit releases all RIDs. Capture: ignored
  build/race_frontend.png. These results validate reconstructed execution, not
  original-executable pixel equivalence.
- The new diagnostic DEV BUILD BY NURUPO label uses the original uppercase font atlas at
  the bottom center, measured using glyph widths with a five-pixel bottom margin
  in the logical 640x480 frame. Only explicit runtime `--debug` enables it; the launcher forwards
  that flag. Native shown/hidden comparisons find 3979 label pixels with the flag
  and zero without it. It is not attributed to original executable behavior.
- Integration testing exposed existing direct RenderingServer shader submission
  of resource preprocessor directives and stage built-ins in shared global helper
  code. Shader variants are now expanded before submission; billboard helpers
  receive the inverse view matrix as an explicit vertex-stage argument. These are
  host shader compatibility repairs, not new RE progress.
