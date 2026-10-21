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

The console starts collapsed. ESC toggles mouse visibility/capture. Clicking its
title expands it; clicking the game background resumes game focus. Driving keys
are ignored while the UI owns focus. Window focus loss releases capture, and focus
return preserves the user's UI/game selection. Closing the window exits the game.

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
console, types an echo command through real engine input events, checks ESC and
background-click focus, and recreates the host three times to check cleanup.
The standalone UI test additionally exercises DPI changes and stream restoration.
