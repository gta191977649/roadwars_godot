# Integrated reconstruction validation

Date2026-10-22. Windows x64, GNU13.2 C++20, Godot4.7.2-stable
ed1daf0bf, Compatibility/OpenGL3.3, Intel Arc B390.
Reference executable SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Original executable and D3DRM.dll were only inspected, not executed or changed.

The independent core and main Godot extension build passed. Core CTest passed
26/26, including scripts, boundaries, generated classes and progress validation.
The original-resource20-second core driving probe passed with14 wall contacts,
5Crash requests,1Land request,4accepted spark bursts and40peak tire particles.
See transmission.md for the final measured RPM/gears and the assembly-backed
per-hit speed and pre-landing wheel-cache corrections.

| Command | Observed acceptance |
|---|---|
| smoke_godot.py |33,282 visible triangle pixels;10 restarts; owned RIDs released |
| smoke_godot.py --fog | Four known-depth vertex-linear fog samples and exempt-material row match; teardown clean |
| smoke_race.py --preview |118,558 original sky pixels;1,577 owned RIDs;3 restarts and teardown clean |
| smoke_race.py |41.29605 displacement,24.62753 speed after acceleration; steering, braking, reset;1,510 skid visibility-difference pixels |
| smoke_race.py --particles |2 sparks,32 tire particles;34,086 visibility-difference pixels;3 restarts and teardown clean |
| smoke_race.py --camera | Three-second manual hold, world heading held while car turns, gradual return, interruption continuity, focus/exit mouse release and real-eye view distance |
| smoke_race.py --audio | Original engine loop advances; idle1033Hz, maximum31512Hz; native mixer signal approximately-4.66dB; voice released |
| smoke_race.py --sfx | Actual driving starts skid loop once and wall Crash3times; isolated tire+collision mixer signal; loop stop;5overlapping one-shots; completion, reset and teardown |

Native SFX acceptance first uses actual W/Space/contact inputs. Diagnostic
injection is used only afterward to isolate native sample playback and verify
simultaneous landing voices. It is not a replacement for gameplay triggers.
Tests use --no-debug-overlay so changing UI counters cannot contaminate pixel
comparisons. Godot log files are checked in addition to captured streams because
the concurrently developed debug console can redirect process output.

Captures and raw native logs remain in ignored build/. Retained result metadata
and source/DLL hashes are in rendering_validation.json. The attempted isolated
acceptance workspace was superseded by successful main-project build/runtime
acceptance after parallel UI sources became available; no isolated test is
represented as a passing substitute for this main-project run.

These results validate reconstructed behavior and native integration. They are
not an original-executable differential comparison. No function is promoted to
verified on this basis. Remaining limitations include full initialization/race
dispatch, pair-collision physics, original weapon/replay effect dispatch, QMDX
spatial attenuation/global channel stealing, rolling tire sound, all camera modes
and orbit-specific obstruction. The original car/car sound contract is tested
independently but has no fabricated caller in the single-car driving profile.
