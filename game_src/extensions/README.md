# Diagnostic extensions

This directory contains new tools, not reconstructed game code. It has no RE
markers and contributes no implemented original functions to the progress registry.

`debug_overlay.*` owns Dear ImGui, the upstream imgui-console widget, its bounded
thread-safe log queue and the render profile. `console_streams.*` tees C++ stdout,
stderr and clog to the widget while preserving the terminal output. These sources
contain no Godot or operating-system APIs and build as a separate optional target;
the portable gameplay core does not require ImGui.

The Godot adapter supplies input, screen DPI and the last completed frame's visible
viewport primitive count. It renders ImGui triangles through RenderingServer canvas
items with per-command scissor rectangles. It captures Godot print/warning/error
messages through a registered Logger. Caught host exceptions stop the failed
simulation and expand the console so the error remains inspectable. Core fail-fast
diagnostics are forwarded before their required assertion/abort; a terminated
process cannot render another UI frame. Native engine crash handling remains owned
by the engine. The adapter does not replace process-wide CRT file descriptors.

The console starts collapsed. The backquote/tilde key (` / ~) or F8 toggles mouse visibility/capture; Escape remains reserved for menus and pause. Clicking its
title expands it; clicking the game background resumes game focus. Driving keys
are ignored while the UI owns focus. Window focus loss releases capture, and focus
return preserves the user's UI/game selection. Closing the window exits the game.

AI races launched with `--debug` additionally supply an owned `RaceDebugSnapshot`
to the same overlay. Race Debug exposes Show AI States, Show Route Debug, Show
Checkpoints and Show Collision Shapes, initially disabled. AI States displays raw
state/substate values alongside evidenced names; absent names remain Unknown.
Its default window sits beside the controls and scrolls horizontally through the
columns. High desktop DPI is capped to fit the actual viewport pixel dimensions,
so controls remain reachable in a small window.
The All AI selector filters participant-owned world primitives, while shared route
geometry remains visible. Options survive scene changes in memory only; clearing
the snapshot hides race windows. The core has no ImGui dependency.

World diagnostics project through explicit eye/right/up/forward camera vectors,
vertical FOV and near distance. Segments clip against the near and four side
planes before perspective division; labels outside the frustum are omitted.
ImGui's background draw list renders these markers without depth occlusion, below
the windows. Route direction arrows, branch colors and contact normals are
diagnostics, not recovered original rendering or game-state inputs.

Route Distance (m) defaults to 200 and accepts 10..2000. Route and target markers
are fully opaque through 80% of that radius around the actual camera, then fade
linearly to zero at the radius. Segments clip to that sphere and subdivide at most
10 metres apart with interpolated vertex alpha; labels follow the same distance
function. Moving closer reverses the fade without hysteresis or simulation state.
Checkpoint and collision diagnostics remain independent of this route limit.

FPS is ImGui's rolling average of host frame intervals; frame time is the current
host interval in milliseconds, not GPU execution time. Triangles come from the
visible viewport's actual primitive counter, not loaded mesh counts. The last
completed render is necessarily reported on the following update. Font raster size
and style dimensions follow the current screen DPI; fonts are rebuilt when it
changes. A system Consolas font is used when available, with ImGui's bundled font
as fallback. ImGui multi-viewport windows are not enabled.

Dependencies are pinned by commit and archive SHA256 in
`scripts/dependencies.lock.json`; `python scripts/build.py deps` fetches them into
ignored third-party directories. Upstream licenses remain in those directories:

- [Dear ImGui](https://github.com/ocornut/imgui), MIT.
- [imgui-console](https://github.com/rmxbalanque/imgui-console), MIT, including its
  bundled [csys](https://github.com/rmxbalanque/csys) headers.

Validation commands:

```powershell
python scripts/build.py godot
ctest --test-dir build/godot -R debug_overlay --output-on-failure
python scripts/smoke_debug_overlay.py
python scripts/smoke_debug_overlay.py --track "Dark Forest"
```

The UI smoke deliberately emits an error and warning, verifies they reached the
console, types an echo command through real engine input events, checks F8 and
background-click focus, and recreates the host three times to check cleanup.
The standalone UI test additionally exercises DPI changes and stream restoration.

`--race --debug` supplies read-only snapshots to Race Debug and AI States windows.
Route Distance defaults200m, accepts10..2000m and fades across the outer20% without
affecting simulation. Debug geometry uses the interpolated camera and clipping.
Viewport-aware DPI keeps controls reachable in small windows. The core does not
require ImGui. See `docs/agent/evidence/competition_validation.md` for acceptance.

`Auto Drive` defaults off and routes player control through the same recovered
RaceAi controller as opponents. Unchecking restores player input on the next
simulation tick; pause freezes both modes. This control changes gameplay, unlike
the four read-only visualization toggles.

`Show AI States` also draws centered screen-projected labels at every vehicle
center, independently of route distance and AI inspection selection. Labels update
from live snapshots and show raw unknown state values plus the recovered branch;
the manually driven player is labeled `Player control`. Near/viewport clipping and
a dark text background keep the overlay legible without changing scene materials.

Inspect defaults to Player:<character>. Selecting an opponent follows its own
portable chase camera and clears mouse orbit once; selecting Player restores the
player camera. This also works while paused. It does not enable Auto Drive or
change AI/physics. Esc opens the original race menu; Continue/Esc resumes and
Restart Race restores the complete starting grid and countdown. Godot SceneTree
pause freezes the scheduler, native audio and RenderingServer particles while
the host continues menu/debug input. Replay remains unavailable.

## Frame pacing graph

The top-right render profile includes a 600-sample wall-clock frame interval graph,
with a green reference line at 1000 / the current display refresh rate. Refresh
rate and runtime VSync mode come from DisplayServer and are polled once per second;
unknown refresh rates and headless output remain explicit in diagnostic metadata. No display or
VSync settings are changed. The adapter samples monotonic microseconds once per
engine process frame, not scaled/clamped process delta or simulation dt. Repeated
manual test draws in one engine frame do not create extra samples.

The compact panel shows mean FPS and the latest interval on one row, a 36-pixel
graph, then nearest-rank P99 and maximum. Padding and row spacing are local to
this panel. Triangle counts, display/VSync details, redundant statistics and
explanatory captions are omitted from the HUD; full timing/display metadata
remains available through the diagnostic interface. At 120 Hz the green reference is approximately
8.33 ms. The graph auto-scales to retain stalls; samples are not clipped or smoothed. Every retained sample is drawn, including
when the graph is narrower than the sample count, so narrow spikes are not skipped.
These are CPU callback-to-callback intervals including scheduling/presentation
waits, not GPU execution times or measured panel scanout timestamps. The debug
interface `get_frame_timing_state` returns chronological retained samples and
summary/display metadata for capture tools. History is bounded in memory and
survives scene restarts with the overlay; sampling does not write a file per frame.

Validation on the configured Vulkan Forward+ host reported 120.00017 Hz and
VSync Enabled. A 20-second real AI race capture retained a final 600-interval
window with mean 8.33163 ms, P95 8.880 ms, P99 15.202 ms, maximum 17.071 ms and
7 intervals above the 12.5 ms threshold. Mean cadence matches the 8.33332 ms
budget while occasional long intervals remain observable; this does not identify
their cause. Ignored build artifacts contain the JSON/CSV history and graph PNG.
The native capture checks repeated same-frame draws and a deliberately injected
100 ms stall. The bounded-history/percentile test, overlay interaction/render
smoke, ten-restart RID lifecycle, portable core tests and boundary/progress checks
passed. No simulation policy or VSync mode was changed by these diagnostics.

Frame attribution and the subsequent resource-preparation investigation are
recorded in `docs/agent/frame_pacing.md`. The graph anchor is now the host callback
entry, before route/debug construction. Use `python scripts/profile_frames.py`
for an opt-in native resource/stage trace; ordinary play does not collect it.

## Frontend unavailable actions

Unimplemented frontend business operations now display a portable recoverable
ASSERTION FAILED / Not implemented modal even without the debug overlay. Enter,
Esc or OK closes it without running the rejected action. Season/Multiplayer,
unsupported option writes, Garage purchases and Replay use this common path.
Single/Quick menu launches and Loading are live; see
`docs/agent/evidence/menu_race_launch.md` for scope and acceptance.
