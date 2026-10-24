# Single Race and shared Garage validation

Validation date: 2026-10-24. Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Original-program evidence in this work is static disassembly/decompilation and
original configuration/resource inspection. Native tests below run the
reconstruction with original assets, not the original executable.

## Delivered behavior

- Single selection, track/lap configuration, Summary, all six Garage stores,
  Loading, live race, pause, restart, terminal spectator modes, time table,
  race-over menu and return to main are connected. Quick Race retains its
  separate generated starting equipment.
- [GarageSession](../game_src/unresolved/garage_session.hpp) provides the shared
  portable transaction interface. [Usage and ownership](garage.md) describe how
  future Season code supplies and retains complete state. There is no added
  inventory, Season schedule or Season persistence service.
- [The reference catalog](garage_catalog.md) lists all 89 product/quality
  records. Quotes and transactions share price, trade-in, slot, repair,
  confirmation and availability rules. Committed equipment drives both Garage
  models and actual race initialization.
- FrontendScene moves the actual DoorTop/DoorBottom boards, swaps products at
  their closing threshold, and emits the swap sound. Quality changes, rapid
  direction changes, purchases during closure, cancellation and reentry have
  explicit tests. Godot only adapts rendering, input and PCM/MS ADPCM audio.
- Replay uses recorded poses and actual combat/visual events, original controls,
  per-owner sample endpoints and native exit side effects. The configured
  REPLAYSECS=0 disables Replay; acceptance explicitly enables a 30-second buffer.
- A post-finish collision regression exposed a missing normalization in the
  reconstructed contact-velocity calculation. Restoring that native operation
  removes the repeated radius multiplication without a velocity cap. Fixed-step
  simulation and interpolation boundaries are preserved.

## Executed checks

Core and Godot extension builds succeeded with `scripts/build.py`.

| Check | Executed result |
|---|---|
| Core CTest | 71 passed; optional installed `single_catalog` skipped in the default 72-test suite |
| Godot `frontend_video_audio` and `debug_overlay` CTest | 2 passed |
| Installed Garage business test | 12,816 quotes and independent transactions; all twelve characters and twelve tracks |
| Installed scene and visual tests | All vehicle/product/quality assets, unavailable meshes, repair/repaint, moving boards, frame cadence and teardown passed |
| Installed menu test | Purchase, cancelled sale, committed race equipment, navigation and lifetime passed |
| Installed `single_catalog` | 144 character/track pairs initialized; countdown, first live tick, equipment effects and restart passed |
| Portable actual-race Replay acceptance | Dark Forest/Animal and Badlands/Kitiara passed; events, seeks, pause, cameras, native exit side effects and 120 resumed ticks |
| Headless frontend and race-menu smokes | All configured pages/variants and six Garage stores passed |
| Native Vulkan competition smoke, seed 4206394798 | Complete race, pause/restart, terminal idle menu, cancellation, spectator cameras, time table, main-menu return and three resource-lifetime reloads passed |
| Native Vulkan Replay smoke | Actual pause-menu entry, seven controls, playback, held rear view, seek and teardown passed |
| Native Vulkan Garage race-menu smoke | Six stores, purchase/cancel, committed models/equipment, Single/Quick, restart/return, continuous music and independent mixer output passed |
| Portable driving frame independence | Eleven render rates plus 350/900 ms stalls; identical 1,200 simulation ticks |
| Portable complete-race frame independence | Eight render rates plus 350/900 ms stalls; complete race traces matched |
| Native host timing smoke | Different render rates and irregular 350/900 ms stalls produced identical 300-tick vehicle/effect state; interpolation, engine pause and lifecycle passed |
| Registry and boundaries | `progress.py update/check`: 9,427 rows; `check_boundaries.py` and `git diff --check` passed |

Local logs and captures remain in ignored `build/`. Notable records include
`single_ctest.log`, `single_catalog_final.log`,
`single_competition_frame_final.log`, `single_driving_frame_final.log`,
`single_native_competition_final.log`, `single_native_replay_final.log`,
`single_native_garage_final.log`, `single_native_timing_final.log` and
`single_installed_menu_final.log`.
The native Garage test checks signal from the mixer independently of music;
issuing a playback command alone is not its audio acceptance criterion.

## Incomplete acceptance and limits

The 144-pair catalog test does **not** finish 144 races: its explicit output is
`completed_races=0`. Complete native race coverage currently uses selected
resources and branches. Exhaustive complete races for every character, track
and lap count, including every elimination path, remain unverified.

Original-program runtime screenshots, frame-by-frame board timing and mixer
captures have not been obtained for direct comparison. The reconstruction's
working audio/animation checks and static evidence do not establish pixel,
timing or audio equivalence to that running program.

Replay has targeted real-event and bounded-buffer tests, but original runtime
equivalence for every overlapping weapon/effect combination, destroyed vehicle
state and long-duration ring/seek case remains unverified. Address-linked
evidence and remaining uncertainties are recorded under
`docs/agent/evidence/single_replay_reconstruction.md`. No claim of fully verified
original Replay equivalence is made.

Original class/source names without layout and call evidence remain unresolved;
`GarageSession` is explicitly a new shared coordinator. Broad original functions
retain analyzing status where only their relevant branches have been recovered.
The progress registry is not advanced merely because a smoke or build passes.
