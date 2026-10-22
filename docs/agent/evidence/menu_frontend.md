# Frontend startup and generic screen reconstruction

Reference program: `RoadWars`, x86 Windows PE, image base `00400000`.
Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.

Read-only decompilations are retained in `menu_frontend_ghidra.json` and
`menu_frontend_support_ghidra.json`. Observations here are static observations;
the original executable was not run. Decompiled parameter types and implicit
receivers are uncertain and are not portable ABI declarations.

## Startup and intentional host adaptations

`004ab60d` calls the publisher AVI wrapper `004ab401` for Avi1 and Avi2, then
creates `Screens/Intro/Logo.bmp` and `Screens/Title/Title.bmp` through `004e19f6`.
Avi1 fills the display. Avi2 is centered with dimensions limited to 320x240.
The Intro bitmap uses the generic screen configuration. Sampled encoded data
provides a three-second Intro timeout and seven-second Title timeout, with
0.3-second bitmap fades. AVI duration comes from its stream time base rather
than from the unused Publisher Intro bitmap configuration.

The original Title timeout returns -1 from `004ae2ad` and dispatches the attract
demonstration through `00406fc0` before returning to Title. The user's explicit
default-launch requirement is to reach the main menu without additional input.
The reconstruction therefore routes the Title timeout to `main_menu`; this is a
documented host adaptation, not an assertion that the original timeout did so.
No attract-mode race simulation is fabricated. Input skipping still advances
through startup pages and uses input edges so a held key does not dismiss every
following page.

`MenuFrontend` is a new portable representation of the frontend portion of the
loop. Godot calls `advance` once per host frame, never enters a nested blocking
message loop, and renders the returned commands. Startup elapsed time is carried
across page and fade boundaries. Existing gameplay retains its independently
authorized fixed simulation policy. Frontend presentation timing is not a claim
that the original gameplay was frame independent.

## Confirmed generic screen behavior

- `004a91f0` reads INFO fields `NUMSELECTIONS`, `DOFADEIN`, `TIMESOUT`,
  `TIMEOUTSECS`, `GOBACKENABLED`, `FADESECS`, and `TITLE`. Each selection supplies
  `VALUE`, `ISAVAILABLE`, and `DOFADE`. Configuration uses the already recovered
  encoded-PSE-first resource path; malformed present PSE is not silently replaced
  with plaintext.
- The same constructor loads `<icon directory>/<VALUE>/16SelectionIcon.bmp` and
  composites localized labels using the original screen fonts. The portable
  canvas emits corresponding texture and glyph quads instead of reproducing
  DirectDraw surface ownership. Exact legacy label compositing is not claimed.
- `004a9fff` loads the shared background, left/right scroll arrows, and navigation,
  selection and unavailable sounds. `004a9879` loads the icon overlay, fade,
  unavailable overlay, dialog and loading textures, plus title, small and Arial
  font atlases. Original asset paths do not establish original source ownership.
- `004ae2ad` places two icons at x=160/480,y=253; three at x=110/320/530,y=253;
  four at x=160/480,y=166/340; five with three top icons and two lower icons.
  Six-item pages use x=110/320/530,y=180/348. Larger selections show six slots
  and scroll across the two rows.
- The selected icon expands from radius 50 to 100; the previous one shrinks toward
  50. The recovered factor is 400 logical pixels per second. The original forces
  a minimum one-pixel change per rendered callback; the reconstruction expresses
  the motion in elapsed time to remove that presentation frame-rate dependence.
  The selection overlay varies between alpha 0 and 0.7; nonselected overlays use
  0.7. Original rounding/minimum-step details are not asserted pixel-identical.
- Navigation helpers `004adecd`, `004ae02b`, `004ae146`, and `004ae1ed` preserve
  row/column boundaries. Disabled icons can receive focus but cannot activate;
  initial focus selects the first available entry. Pointer hover is supported by
  the host adaptation; this does not establish original mouse-hover behavior.
- `00460fdf` supplies elapsed/fade-duration opacity for page entry and exit.
  The portable frontend retains finite, validated durations and clamped opacity.

## Screen coverage and business boundary

The generic controller covers these configured icon-page families:

| Portable screen | Original configuration / icon directory |
| --- | --- |
| main_menu | Screens/SelectGameOption / Game Options |
| select_option | Screens/Select Option / Options |
| select_controller | Screens/Select Controller / Controllers |
| select_character | Screens/SelectCharacter / Characters |
| select_track | Screens/SelectTrack / Tracks |
| season_type | Screens/Select Season Race Type / Season Types |
| select_league | Screens/Select League / Season Leagues |
| select_connection | Screens/Select Connection / Connections |
| host_or_join | Screens/Host Or Join / Host Or Join |

Startup also covers publisher_1, publisher_2, intro and title; the original info
bitmap is independently previewable. Options, controller-binding rows, season
lists, sessions, summaries, results, standings, loading, confirmations and pause
have custom presentation contracts described in `menu_custom_screens.md`.
Publisher video/audio are described in `frontend_video.md`; localization tables
and font alphabets in `frontend_localization.md`. Garage and victory/defeat
require their separately evidenced 3D presentation and are not considered complete
merely because a screen ID exists. Read their current implementation evidence
before reporting full coverage.

`004ab60d`, `004aac8f`, `004b67bb`, and `004b4a35` establish the broad main-menu,
single-race, season and multiplayer page routes. Presentation navigation is
implemented separately from business dispatch. Race launch, save/load persistence,
network enumeration/join and live results are not faked. Business activation
returns an observable unavailable-action description. Preview racer/session/
season data are labeled explicitly and are not claims about live game state.
Options remain in-memory preview values. Quit terminates the host lifecycle.

Preview accepts a screen ID, a nonnegative finite animation time and an explicit
variant. It freezes automatic timeout routing while preserving page drawing and
animation. Production start uses the original installed resources; missing fonts,
bitmaps, videos and malformed configurations return failure. Godot owns input,
filesystem, native codec/audio and RenderingServer resources; no platform type
enters the frontend core.

## Verification and remaining fidelity limits

See [frontend_validation.md](frontend_validation.md) for integrated acceptance
commands, screenshot coverage, lifecycle results and outstanding verification.

`tests/core/menu_frontend_test.cpp` includes synthetic checks for startup time
carry across bitmap pages, one-shot versus 120-FPS startup, exact fade boundaries,
preview timeout suppression, held skip/accept, zero-duration input edges, grid
navigation, selected/unselected radius animation, disabled activation, malformed
counts/durations/selections, encoded-config precedence and texture teardown.
The original-asset mode compares the complete 31-second startup in one callback
against 1860 callbacks at 60 FPS, then previews every generic screen.

On 2026-10-22, `cmake --build --preset core --target roadwars_menu_frontend_test`
completed and both the synthetic invocation and invocation against the configured
installation passed, including the full-startup time comparison and repeated
start/texture teardown assertions. Native Godot screenshot and RID validation
are separate host acceptance checks; this portable test does not establish them.

These checks validate reconstructed execution and memory/resource contracts.
They do not prove original-executable visual equivalence. Full localization and
original legacy label compositing, all custom-screen dynamic fields, and complete
3D garage/victory behavior must retain the precise limitations recorded in their
respective evidence. A mapped source marker alone never promotes a containing
original function to implemented or verified.

## Single Race versus Quick Race routing audit (2026-10-22)

Reference SHA256 is unchanged and was rechecked against the installed artifact.
Read-only live Ghidra results are retained in single_quick_menu_flow_ghidra.json.
These are static control-flow observations, not an original runtime recording.

- 004ab60d sets race+0x24 to 1 for SINGLE RACE, 0 for QUICK RACE, then calls
  004aac8f for both. SEASON RACE calls the separate 004b67bb handler.
- 004aac8f calls 004b10d2 (Screens/SelectCharacter), then the generic
  Screens/SelectTrack screen. Both modes select character and track first.
- Assembly 004aae64..004aaedc tests race+0x24. Single Race calls 004927c0,
  which loads Screens/StartRaceSummary/Data.ini. Quick Race bypasses that call,
  supplies its start action and consumes a rand()%2 draw instead.
- After accepting the start action, 004ab23f onward calls 004aa5f3 (LOADING),
  then 004a4f79 creates the six vehicles, calls 004a55a9 and 00484189 with
  Game Options/Single Race/RaceInit. Loading is after selection/summary.
- 004a55a9 always randomizes opponent equipment/paint. Player randomization is
  conditional on its caller flag or game+0xeec; 004a4f79 passes race+0x24==0.
  Consequently Quick Race requests player randomization; Single Race can retain
  the player vehicle configured through Summary/Garage. Uniform randomization
  of every player configuration is not an unconditional original Single rule.

Current reconstruction audit: MenuFrontend::activate merges Single/Quick into
select_character -> select_track -> start_race_summary. This is consistent with
Single's screen order, but incorrectly omits Quick's bypass. GameHost's frontend
loop advances/draws the menu and does not consume a race launch request; Summary
START remains an unavailable business action. It is not running the season race
handler. CLI --race independently starts the existing live diagnostic RaceSession
and randomizes all participants; it is not a completed menu Single Race business
flow. This audit changes no menu routing or garage state and does not claim those
missing transitions are implemented.

## Live Single/Quick implementation follow-up

The audit's missing menu launch is now connected; see menu_race_launch.md for
actual flow, stock versus randomized player configuration, Loading, menu BGM,
executed acceptance and remaining Garage economy/legacy ABI limits. The earlier
blanket statement that all race launch business actions are unavailable no longer
applies to Single/Quick Start Race.
