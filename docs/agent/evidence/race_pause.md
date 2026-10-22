# Race pause, restart, route height and spectator inspection

Target SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static Ghidra evidence is retained in race_pause_ghidra.json. No original runtime
execution or comparison is claimed. New pause/spectator mechanics remain host
integration and portable presentation, without invented original class ownership.

## Original pause and restart

004ac5a6 reads Game Menu/<variant>.ini and Screen Setup.ini, captures the underlying
frame, dims it and centers the selection panel. Single Race.ini contains CONTINUE,
RESTART RACE, REPLAY, EXIT TO MAIN MENU, EXIT GAME, all originally available. The
existing frontend now supports a live pause overlay over frozen race HUD/world,
with original textures/fonts, spacing, opacity and row gradient. Keyboard arrows,
Enter, Escape and mouse selection work. Pointer warp on capture release is ignored.

0045e555 compares the returned RESTART RACE command and calls004834ea;004834ea
calls0048355e(0). Live restart reuses portable RaceSession::restart and the existing
reset presentation path, resetting all racers, countdown, progress, times, ranking,
AI, camera acquisition, sound cues, native voices and interpolation. No synthetic
lap completion occurs. Continue/Escape resumes. Exit to main menu tears down race
owners before constructing the main frontend; Exit Game requests host shutdown.
Replay remains an explicit unavailable business action; its engine is not invented.
Full original dispatch/vehicle reset mappings remain analyzing.

## Godot pause and frozen presentation

The previous session-only pause still advanced GameEntry's fixed clock; changing
alpha repeatedly interpolated previous/current poses while simulation was frozen.
The host now uses SceneTree::set_pause, keeps its input/menu/debug dispatcher in
PROCESS_MODE_ALWAYS, and skips GameEntry::advance while the pause menu is active.
Native AudioStreamPlayers use PROCESS_MODE_PAUSABLE. RenderingServer particles
also receive speed_scale0 because they are not scene nodes. Wall-clock sampling
continues to avoid accruing pause duration as simulation debt. Existing time debt
and interpolation alpha remain unchanged across pause/resume. This is the requested
modern Godot policy, not a claim that the original used an engine SceneTree.

## Route-height correction

Dark Forest's decoded node16 center is(-275.612,-244.313,913.327); node21 center
is(-383.914,-244.136,907.885). Some route centers lie outside the surface grid.
The previous diagnostic ground projection kept serialized Y when no triangle was
found, creating the screenshot's downward fold. Display-only projection now searches
real nearby surface triangles within16 world metres if the exact XZ sample fails.
It never uses raw route Y as ground. If no surface is found, the vertex is marked
nonfinite and omitted by projection rather than drawing an invented underground
segment. Original XZ, node order, branches, steering targets and simulation data
are unchanged. This fallback is new diagnostic presentation, not recovered AI.

## Inspect camera

Inspect now lists Player:<character> then each live AI. -1 means player/default;
nonnegative values select an opponent. Each racer has independent portable chase
presentation state advanced on fixed ticks without consulting the UI selection.
Godot submits the selected snapshot; a selection change clears orbit and visual
interpolation once, including while paused. No control ownership, AI RNG or physics
state changes. Player Auto Drive remains an independent checkbox. Route filtering
still uses the selected AI, with the player entry showing the overall route graph.
