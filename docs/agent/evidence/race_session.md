# Portable no-weapons competition

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only decompilation is retained in `race_session_ghidra.json` and
`race_setup_ghidra.json`, together with
[route](race_route.md), [AI](race_ai.md) and [pair collision](race_collision.md)
evidence. No original executable was run. Tests execute reconstructed code.

## Ownership and original fields

`004a4daf` allocates a race object through `0047f790` and selects player slot5.
`004a4f79` allocates six vehicle objects through `0040cfd0` and loads each selected
character. `00484189` links their track pointer at vehicle+4418, stores the
participant array at race+034c with count+0348, and selects controller ownership.
These confirm object relationships, not original C++ class names. `RaceSession`,
`RaceParticipant`, `RaceAi` and `RaceRoute` are explicitly portable inferred owners
under unresolved; no invented class identities are registered in the inventory.

`004a527a` reads available characters from Screens/SelectCharacter/Data.ini,
excludes the player, chooses by rand modulo remaining count and removes each
selected entry. The diagnostic entry uses the original CRT recurrence from
`005170b0`, with an explicit repeatable seed1. This seed is a new diagnostic
default; full legacy startup RNG consumption, weapon randomization and timer
seeding are not claimed identical. Reduced AI counts are also a new debug option.
The player occupies the last requested grid slot; six-car racing uses original
slot5/Start6. Each car retains its own parameters, physics, controller and progress.

`0048355e` resets ten lap-time slots at vehicle+4440 and five progress gates,
places racers at Start(index+1), initializes the current route quad with
`004be8f9`, resets controls and reads SECONDSTILLGO through `00484189` from
Game Options/Single Race/RaceInit.ini (installed value9). The CLI accepts1..10laps.
Configuration keeps the existing PSE-first contract and never writes original data.

## Scheduling and results

`00480b40` delegates single-race updates from `004a4f57`. Reconstructed competition
uses separate vehicle begin, wall and finish phases: controller/force integration
for all vehicles; `0047fff6` wall/higher-index pair passes; up to ten affected-pair
neighbor rechecks using remaining callback time; then suspension/transmission and
effect completion. The original32-entry worklist capacity is checked explicitly;
overflow fails rather than corrupting memory or silently dropping contacts.

AI sees earlier racers' newly integrated poses in the original sequential order.
Obstacle scans refresh one racer per round-robin update. A recovery relocation
updates the saved previous pose before physics, preventing a fabricated checkpoint
sweep. Missing-ground recovery uses original route sampling; it never adds laps.

The modern60Hz clock is shared with existing gameplay, preserves catch-up debt and
feeds interpolated visuals only. The old free-driving path calls the same three
vehicle phases consecutively, preserving its previous behavior. Pausing freezes
race/AI/effects while debug UI remains usable; R restarts the diagnostic competition.

`0047fd27` preserves finish order and sorts remaining cars by lap and guarded route
percentage. Live HUD uses actual ranking, lap counts and map markers. On player
finish the host captures real race results, releases the competition, switches
GameEntry to frontend wall-time scheduling and opens the original result layout.
Unfinished racers are DNF. The debug entry does not award/save money or points;
the result page explicitly states REWARDS NOT APPLIED and displays finish times.
Continue returns to the main menu. Preview fixture data never replaces live results.

## Diagnostics and limits

Only `--race --debug` supplies race snapshots to the existing optional ImGui
extension. The core has no ImGui, Godot or OS dependency. Snapshots contain raw
AI mode, diagnostic branch names, controls, recovery timers, route/target nodes,
progress and actual collision geometry. Unknown original enum names remain Unknown.
Debug reads neither consume randomness nor mutate simulation state.

Route debug vertices are projected onto sampled road height because Lap.bin has
non-ground Y values that original XZ helpers intentionally ignore. This affects
copied debug vertices only. Route Distance defaults200metres, adjustable10..2000;
the outer20% fades linearly to zero around the actual interpolated camera.
Distance clipping/fading, UI layout, seed choice and reduced grids are diagnostic
infrastructure, not recovered original rendering or race rules.

Weapons, damage, destroyed-car interactions, equipment randomization, network
events, full legacy object caches and exact original SIMD/x87 approximation remain
outside this subset. Opponent engine spatialization and complete opponent particle
dispatch are not claimed restored. Countdown text uses the existing HUD atlas;
the original full starting-camera choreography remains partial. All broad original
functions remain partial/analyzing; successful races do not establish full binary
or pixel equivalence. See [competition_validation.md](competition_validation.md)
and [race_core_validation.md](race_core_validation.md) for validation results.

## Grid projection correction (2026-10-22)

The initial competition reconstruction copied raw Start dummy Y. This omitted
0048355e's call to 004bb389 for every slot. Existing retained decompilation in
race_setup_ghidra.json and track_collision_ghidra.json confirms surface projection
followed by Y minus0.5. RaceSession now applies this for all requested grid slots
and fails explicitly if the surface is absent. Start XZ, heading and slot order
remain from the original world resource. This corrects artificial startup falling;
it does not invent a settling loop or teleport during race acceptance.

Default Dark Forest projected Y for Start1..6 is3.57632,4.32905,5.44989,6.91541,
8.80227,10.3622; sampled road Y is exactly0.5 higher. Portable original-asset
acceptance checks every grid position before advancing simulation and after
restarts. RWA Speedway/Sarge six-car acceptance also passed.
