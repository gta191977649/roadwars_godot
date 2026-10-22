# Frontend acceptance record

Date: 2026-10-22. Reference SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Evidence addresses and coverage are in [menu_frontend.md](menu_frontend.md),
[menu_custom_screens.md](menu_custom_screens.md), [frontend_scene.md](frontend_scene.md),
[frontend_video.md](frontend_video.md) and [frontend_input_names.json](frontend_input_names.json).
The original executable was not run. These tests validate the reconstruction;
they do not establish pixel equivalence with an original runtime.

## Passed checks

| Command / check | Observed result |
| --- | --- |
| `cmake --build --preset core` | Portable C++20 build passed. |
| `cmake --build --preset godot` | Core, extension and platform tests built. |
| `ctest --preset core --output-on-failure` | 34/34 passed. |
| `ctest --preset godot --output-on-failure` | 36/36 passed. |
| Original-installation frontend tests | Both AVI files, 303 decoded frames; eight localization tables; nine font atlases; custom layouts and stores; 3D cameras/ownership; full startup timing/navigation passed. |
| `python scripts/smoke_frontend.py --default-config` | Native startup from configured installation, 32 screen IDs and 29 additional variants; navigation, animation samples, resize and three restarts passed. |
| `python scripts/smoke_frontend.py --headless --default-config` | Same presentation paths and lifetime checks passed without leak warnings after audio teardown fixes. |
| `python scripts/smoke_race.py --frame-independent` | 1200 ticks identical at 10/15/30/60/75/120/144/240 FPS and jitter/stalls; elapsed time retained. |
| `python scripts/smoke_race.py --timing` | Native host timing, stalls, time scale zero and lifecycle passed. |
| `python scripts/smoke_race.py --frontend` | Existing driving HUD, movement, resize and restart checks passed. |
| `python scripts/smoke_race.py --audio` | Native original PCM, pitch/gain and voice release passed after shared audio teardown change. |
| `python scripts/smoke_godot.py`, also `--headless` | Diagnostic rendering/lifecycle passed, including ten restarts and RID release. |
| `python scripts/smoke_billboard.py` | Native 24-view billboard check passed. |
| `python scripts/smoke_debug_overlay.py` | Native debug overlay passed; expected logger error fixtures were exercised. |
| `python scripts/progress.py update`, `check` | 9427 registry rows validated; 34 frontend mappings retained as partial/analyzing. |
| `python scripts/check_boundaries.py`, `git diff --check` | Passed. |

Native captures are generated in ignored `build/frontend_captures/`: one image
per ID/variant, a clean default main menu, three focus animation samples and a
wide viewport. They cover six store categories, four connection types, eleven
pause variants, DNF/join/empty/loading states and two additional dialog questions.
Visual inspection included main/options/results/summary, garage/store products,
victory podium/prizes and retained dialog backdrops. An initially reversed 3D
camera heading was corrected and covered by an orientation regression.

Audio players explicitly detach streams before deletion. The smoke harness yields
one frame between restart cycles so AudioStreamPlayer consumes queued starts,
and allows a mix callback after freeing the host. Immediate same-frame restart
and destruction produced intermittent Godot WAV playback references before this
sequencing correction; repeated final headless runs passed.

## Outstanding verification and fidelity limits

`python scripts/smoke_debug_overlay.py --headless` timed out after 60 seconds.
Its failure reporting path awaits a rendered frame under the dummy renderer.
The native equivalent passed. This headless check is not reported as passing.

Dynamic garage flares, original character animation, full colored lighting,
damage/equipment/ownership-dependent fields, some non-English custom labels,
editable name/cheat/chat widgets and multiplayer ready indicators remain partial.
Legacy device enumeration is unavailable. Original label compositing is replaced
by original-atlas glyph quads. See individual evidence for exact limits.

Title timeout intentionally reaches the main menu rather than launching the
original attract race, as required by the requested default startup behavior.
Camera presentation uses a deterministic 60 Hz clock with retained debt.
Race/network/save/purchase execution is intentionally unavailable; dependent views
use explicitly labeled fixture data. These scope decisions do not justify
promoting containing original functions to implemented or verified.
