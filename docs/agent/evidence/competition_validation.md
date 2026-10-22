# Competition integration acceptance

Date: 2026-10-22. Reference SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Tests execute the reconstruction with original resources. The original executable
was not run; these results do not establish original-runtime equivalence.

## Passed checks

| Check | Observed result |
| --- | --- |
| Core/Godot builds | Both presets built successfully. |
| `ctest --preset core --output-on-failure` | 38/38 passed. |
| `ctest --preset godot --output-on-failure` | 40/40 passed. |
| Complete competition / frame matrix | Default six-car three-lap race:302.05 seconds,18603 ticks; exact per-tick state at10/15/30/60/75/120/144/240 FPS and350/900ms stalls, including debug-read purity. Zero-AI and RWA Speedway/Sarge passed. See [core record](race_core_validation.md). |
| `python scripts/smoke_competition_godot.py` | Native complete race, real results and Continue to main menu; actual input for five checkboxes, backquote/F8 focus (Escape never changes mouse focus), Escape pause/resume and resize; three restarts and zero owned RIDs at exit. |
| Same command with `--laps 3` | Default six-car three-lap configuration also finished in the native host, displayed live results and passed navigation/restart/cleanup checks with all debug overlays enabled. |
| Same command with `--headless` | Complete race, UI/state/navigation and cleanup passed without leak warnings; no pixel claim. |
| Same command with `--headless --without-debug` | Startup creates no debug UI/RIDs, then clean teardown. AudioServer is allowed mix callbacks around teardown. |
| ImGui contracts | Checkbox interactions, AI filtering, frustum/near clipping, distance alpha, sphere clipping and reverse camera movement passed. |
| `python scripts/smoke_race.py --frame-independent` | Existing1200-tick driving traces remained identical across eight frame rates and jitter/stalls. |
| `python scripts/smoke_race.py --timing` | Native fixed timing, interpolation, stalls, engine pause and lifecycle passed. |
| `python scripts/smoke_race.py --frontend` | HUD textures, movement/reset, resize and three restarts passed. |
| `python scripts/smoke_race.py --audio`, `--sfx` | Native PCM, actual tire/collision triggers, overlapping voices, reset and teardown passed. |
| `python scripts/smoke_debug_overlay.py --track "Dark Forest"` | Native console, expected logger fixtures, input/focus and three restarts passed. |
| `python scripts/smoke_frontend.py --default-config` | Default startup,32 IDs,29 additional variants, navigation, resize and restarts passed. |
| `python scripts/smoke_godot.py`, also `--headless` | Diagnostic ten-restart lifecycle passed; native triangle capture passed. |
| `python scripts/smoke_billboard.py` | Native24-view winding, atlas and camera checks passed. |
| Progress, boundaries, whitespace | Update/check validated9427 rows;29 race mappings retain partial/analyzing status. Portable boundaries and `git diff --check` passed. |

Native captures stay in ignored `build/competition_captures/`: countdown, debug
controls, wide viewport, moving race and live results. Inspection confirmed
readable ImGui text, moving player geometry, real AI modes, HUD and route fade.
Results show real finish times/DNF without PREVIEW DATA and explicitly state that
debug racing does not apply rewards.

## Acceptance corrections

- Corrected AI0047c820 to XZ distance, resolving a later-lap stall; added a
  height-invariance regression.
- Split vehicle begin/wall/finish phases for shared original collision dispatch.
  Free driving retains consecutive phases.
- Accepted original AI's tiny negative brake values in visual wheel-spin input.
- Made ImGui canvas command order explicit to prevent table backgrounds covering
  text; viewport-aware DPI and side-by-side windows keep checkboxes reachable.
- Released sky material/shader RIDs independently when the dummy renderer returns
  an invalid sky RID alongside valid material/shader resources.

## Remaining limits

See [session](race_session.md), [AI](race_ai.md), [route](race_route.md) and
[collision](race_collision.md) evidence. Original class names/source paths remain
unresolved; portable owners retain address mappings. Weapons, damage, network and
persistence are outside the chosen scope. Exact SIMD/x87 approximations, original
starting-camera choreography, caches, opponent audio spatialization and complete
opponent effects remain partial. Acceptance does not promote containing original
functions to fully implemented or verified.

## Debug control follow-up (2026-10-22)

Host-only bindings now use the physical backquote/tilde key or F8 for debug mouse
focus. Escape press and release are consumed by competition pause and never change
debug mouse focus. This is an integration change, not an original input claim.

Race Debug adds a default-off `Auto Drive` checkbox. The host passes this explicit
control to RaceSession::set_player_autopilot before each simulation tick. It selects
the same RaceAi::update and wall_update paths used by opponents, including normal
physics, collision and checkpoint processing. Disabling restores player controls
on the next tick. This does not upgrade the reconstruction completeness of the AI
functions documented in race_session.md and the associated AI evidence.

Revalidated core build and 38 tests, Godot build and 40 tests, native and headless
competition smoke, and native console smoke. Competition smoke clicks all five
controls, toggles Auto Drive off/on, completes the race with the test-only autopilot
argument false, checks backquote/F8 and Escape isolation, then exercises restarts
and resource release. The initial console smoke failed command entry during a
concurrent validation run; its isolated rerun passed. Console logging deliberately
emits its expected warning/error fixtures. Progress and boundary checks passed.

## Vehicle state labels (2026-10-22)

Show AI States now also enables a live label at every vehicle's simulation center,
including the player. Labels use the current raw behavior number (Unknown),
recovered controller branch, character and finish flag; manual player control is
explicitly identified. This is new diagnostic presentation, not a recovered
original visual effect. Selection and route-distance controls do not hide these
labels. Existing camera projection clips the near plane and viewport; centered
white text has a translucent dark background and updates from read-only snapshots.

Core and Godot builds passed; core38/38 and Godot40/40 tests passed. Native
competition smoke completed a real AI-driven race, results, navigation, three
restarts and cleanup. Inspected build/competition_captures/race_running.png:
player and opponent state labels follow visible vehicle centers. Progress and
boundary checks passed. No function completeness status changed.

## Collision overlay, route topology and starts (2026-10-22)

Collision outline diagnostics previously reused wall footprint vertices flattened
to Y=0. They now use the actual pair solver's XZ footprint (0041d580) and lift only
the display copy to the vehicle Y. The recovered solver remains flat as required
by0041e5bd; collision behavior is unchanged by this visualization correction.
Contact normals already carry vehicle contact elevation. This is a two-dimensional
solver outline, not an invented three-dimensional bounding box. Snapshot geometry
represents the latest fixed tick, while scene/camera presentation interpolates.

Route edges follow serialized ascending indices and wrap final to first, matching
RaceRoute's forward sample/index contract (004182ee). Main-to-branch entry edges
were missing when only the next node had a branch. Diagnostics now include these
along with branch continuation and exit; arrows point from each edge's source to
its destination. Labels use zero-based runtime indices; resource IDs are one-based.
ImGui draws the overlay above the world, below its windows, with viewport/near
clipping. No scene depth ordering is claimed for this intentional debug overlay.

Tests check resource edge endpoints/order/wrap, collision XZ against solver
footprints and vehicle Y, and all initial grid heights. Core38/38 and Godot40/40
passed. Default one-lap six-car full frame matrix passed at10..240FPS with350/900ms
stalls and pure debug reads; corrected spawn now yields108.717s/7003ticks. Older
race timings above predate the grid correction. RWA Speedway/Sarge acceptance and
native six-car competition/results/restarts/cleanup passed. Inspected native
race_running.png with collision outlines at vehicle elevation. An initial native
focus smoke failed while other checks ran; isolated repeat passed. Progress and
boundary checks passed. Original-runtime equivalence is not claimed.

## Race start presentation follow-up (2026-10-22)

See race_start.md and race_start_ghidra.json for the restored StartCam sequence,
chase acquisition, countdown scaling/fade and original3/2/1/Go samples. Dedicated
Fonts/16321go.bmp with alphabet321G replaces the initial ordinary HUD font mistake;
G is the full GO! graphic. Native clean captures start_clean_20/24/28/32.png were
visually inspected. Core38 and Godot40 checks passed, full fixed-tick camera/cue
frame matrix passed, native competition and no-debug intro lifecycle passed.
