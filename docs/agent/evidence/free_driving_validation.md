# Free-driving integration validation

Historical first-profile acceptance. Later rendering/particle corrections and current
coverage are recorded in ../rendering_corrections.md and rendering_validation.md.

Date: 2026-10-22. Windows x64; MinGW GNU C++ 13.2.0; Godot
4.7.2.stable.official.ed1daf0bf; locked godot-cpp API 4.7.
Original input SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
The executable was read through Ghidra and static byte inspection, not executed.
All original assets were read from the ignored local configuration's game_dir.

## Executed acceptance

| Check | Observed result |
| --- | --- |
| Portable core build, Godot disabled | Passed |
| Godot extension build with pinned dependency | Passed |
| Core CTest | 13/13 passed |
| Godot-build CTest | 13/13 passed |
| Python script tests within the suite | 12 passed, including local configuration and trace argument forwarding |
| Progress consistency and dependency boundaries | Passed; 9,427 rows; no generated class changes |
| Original asset decode | 1,814 archive records; Dark Forest 348 objects/30,891 vertices; Animal Body 18 objects/726 vertices |
| Original surface/spawn probe | 15,288 surface records; Start1 and friction sample match retained track evidence |
| Portable original-asset driving probe | 1,200 updates, 20 simulated seconds; acceleration, steering, braking, exact reset and finite state assertions pass; three actual wall contacts |
| Diagnostic RenderingServer smoke | 33,282 visible triangle pixels; ten restarts; all owned RIDs released |
| Original-asset RenderingServer scene | 1,559 owned RIDs; three restart cycles with stable count; zero after exit |
| Godot frame-loop driving smoke | 43.1080 units displacement after three seconds of throttle; speed 28.1403; steering, braking and reset checks pass; zero RIDs after exit |

Render captures are generated at build/race_preview.png and build/race_drive.png.
Visual inspection confirms an original textured track and Animal viewed by the test
chase camera, including vehicle pitch/roll pose. Captures and original assets are
not versioned. The sky, original lighting and animated tire poses remain incomplete.

The 1/60-second probe input is a harness choice, not an imposed runtime physics rate.
The runtime accepts Godot elapsed time through the original outer-loop clamp.
The PowerShell wrapper's quoted arguments are covered by launcher tests; actual
scene acceptance invokes the same extension and profile through smoke_race.py.

## Completion interpretation

The registry contains 35 implemented helper functions, 19 analyzing functions and
zero verified functions in the current set of 54 explicitly required functions.
Thousands of inventory entries still have undecided scope. These percentages only
describe that narrow set; they do not estimate whole-game completion. Full
`0045c8ab`, vehicle/race dispatch, missing-ground respawn and partial readers remain
incomplete. No whole class or original-runtime differential equivalence is verified.

See test_driving.md, shared_initialization.md, vehicle_dynamics.md,
contact_orientation.md, track_collision.md and wall_response.md for exact scope,
addresses and remaining branches. Unknown behavior must retain trace/assert.
