# RoadWars: portable logic with Godot rendering

Reconstruct the original RoadWars behavior in platform-independent C++, with Godot 4.7.2 providing the host and RenderingServer rendering through GDExtension. An explicit original-asset free-driving test profile now loads Dark Forest and Animal. This is an early reconstruction, not a complete game port.

## Layout and direction

- `game_src/`: portable reconstructed logic. Recovered files follow evidenced original source/module/class organization where available. `core/` and `interfaces/` currently contain explicitly new host infrastructure. `unresolved/` is reserved for recovered code whose original location is not established; do not invent gameplay module hierarchies.
- `platform/godot/`: all Godot code, project, RenderingServer adapter and pinned godot-cpp dependency.
- `docs/agent/`: agent instructions, architecture, reverse-engineering evidence and standards.
- [Progress](docs/Progress.md) and [registry](docs/functions.csv): all known original functions and types, with separately tracked scope, implementation and verification.
- `scripts/`: build, validation, inventory and progress automation. `tests/`: portable contract and tracking tests.

The original executable is the behavioral reference. Recovered vehicle force helpers and original ground/wall collision run in portable C++; no Godot physics is used. AI and race rules remain unavailable. New infrastructure is not counted as recovered functions. No neighboring RoadWars project has been imported. Default launch runs the frontend; trace mode selects the free-driving profile. Full original initialization and race dispatch remain partial.

## Frontend

After selecting or detecting the original installation, normal launch plays both publisher videos,
the developer logo and Title, then enters the animated main menu:

```powershell
python scripts/build.py --run
python scripts/build.py --run --frontend-screen modify_vehicle --frontend-variant tires --frontend-time 2
python scripts/build.py --run --frontend-screen race_results --frontend-time 4
python scripts/build.py --run --diagnostic
python scripts/smoke_frontend.py --default-config
```

Arrow keys select, Enter activates page navigation and Escape returns. Store item and
quality values change with Left/Right. Business actions such as starting a race,
purchasing, saving or joining a session report unavailable and do not execute.
State-dependent screens show explicit preview data. The backquote/tilde key or F8 focuses the optional `--debug`
overlay. The core owns page state, animation, layout, Cinepak decoding, menu cameras and
3D presentation; Godot owns rendering and audio playback. Publisher GSM audio uses a
Windows host codec adapter; other hosts report that codec unavailable.

The frontend currently has 32 preview screen IDs, six store categories, four connection
views and eleven pause-menu variants. See [frontend evidence](docs/agent/evidence/menu_frontend.md),
[custom screens](docs/agent/evidence/menu_custom_screens.md) and
[3D screens](docs/agent/evidence/frontend_scene.md) for addresses and fidelity limits.
The [acceptance record](docs/agent/evidence/frontend_validation.md) lists tests,
capture coverage and remaining gaps.

## Drive the test scene on Windows

The no-weapons competition debug entry supports original AI, car collisions,
checkpoints, laps, ranking and a live result page:

```powershell
.\build.ps1 --run --race --track "Dark Forest" --laps 3 --car animal --ai 5 --debug
.\build.ps1 --run --race --car "Avenging Angel" --laps 1 --ai 3
```

Defaults are Dark Forest, Animal, three laps and five AI. Laps accept1..10 and AI
accepts0..5. Escape pauses, R restarts and The backquote/tilde key or F8 focuses the optional debug UI.
With `--debug`, Race Debug provides AI States, Route Debug, Checkpoints and
Collision Shapes switches. Route Distance controls visibility with a smooth fade
over the outer20% of its range. Options remain in memory; debug reads never change AI.
See [competition evidence](docs/agent/evidence/race_session.md) for ownership,
original addresses, intentional diagnostic additions and remaining limits.
The [acceptance record](docs/agent/evidence/competition_validation.md) lists tests
and capture coverage.

Copy `config.example.json` to ignored `config.local.json` and set `game_dir` to the original installation containing Data.pak/Data.pkh, and `godot_executable` to Godot 4.7.2. Only explicit `--debug` launch uses this developer resource root; ordinary launch detects assets or asks for the installation directory.

```powershell
python scripts/build.py deps
.\build.ps1 --run --debug trace "Dark Forest"
.\build.ps1 --run --jobs 8 --debug trace "Dark Forest"
```

`--jobs N` sets CMake build parallelism for `core`, `godot` and `trace` (default4).
Use a positive integer; the option can appear before or after the action.

`trace` takes a track name, quoted when it contains spaces. W/Up accelerates, S/Down brakes then reverses, A/D or Left/Right steers, Space applies full braking, R resets to Start1, and the backquote/tilde key or F8 toggles mouse focus. Move the mouse to orbit; after three idle seconds it gradually returns to the recovered chase camera. The undamaged profile uses stock Street/Poor tires and original automatic shifting when MANUALTRANSMISSION is disabled. Original torque, gear/RPM and grip formulas run in portable C++; no extra acceleration slowdown is applied. Original engine PCM loops through Godot audio. Original texture/alpha rules, sky, regional view distance, linear vertex fog, wheels, sparks, tire smoke/dust and skid strips use native Godot rendering. AI, weapons, damage, laps and original menus remain incomplete; missing-ground respawn still traces/asserts. Mouse orbit and reset are new conveniences. See [profile coverage](docs/agent/test_driving.md) and [rendering audit](docs/agent/rendering_corrections.md).

```powershell
python scripts/smoke_race.py --preview
python scripts/smoke_race.py
python scripts/smoke_race.py --core
python scripts/smoke_race.py --particles
python scripts/smoke_race.py --camera
python scripts/smoke_race.py --audio
python scripts/smoke_race.py --sfx
python scripts/smoke_godot.py --fog
```

## Class scaffolds and original entry loop

Sliding sound follows the original first-tire material and grip gate, with a
continuous loop until all tires stop skidding. Wall crashes and wheel landings
use the original Crash/Land samples and independent one-shot voices. The original
car/car sound threshold is recovered, while the single-car profile has no pair
collision solver. QMDX spatial attenuation/channel stealing remain outside the
native nonspatial playback adapter. Side weapon mounts begin hidden; weapon
deployment is still unavailable. See the evidence files for exact coverage.

```powershell
python scripts/generate_classes.py
python scripts/progress.py update
python scripts/generate_classes.py --check
python scripts/build.py --run --original-loop
```

Generated output currently covers 132 confirmed types and their 262 associated functions. Original source directories remain unconfirmed, so files use `game_src/unresolved/types`. Most known RTTI types are library types; unassigned gameplay functions remain unresolved. Each stub prints original identity and source location, asserts, and aborts even in release mode. Strict launch intentionally stops at the first unimplemented original-loop stage. Default launch runs the frontend; the triangle requires `--diagnostic`.

Read [the generation guide](docs/agent/class_generation.md) before implementing or regenerating. Hand edits are protected; stub markers never count as completed functions. The goal is to implement and verify every required gameplay class member as ownership and signatures are recovered.

## Requirements and build

Use Python 3.10+, CMake 3.24+, Ninja, a C++20 compiler (MSVC or MinGW on Windows), and Godot **4.7.2-stable**, standard single precision. Run from the repository root. No Git repository or submodule is required.

```powershell
python scripts/build.py core
python scripts/build.py test --preset core
python scripts/build.py deps
python scripts/build.py godot
python scripts/build.py test --preset godot
```

`./build.ps1` (or `python scripts/build.py`) builds the Godot extension incrementally
without launching it. Add `--run` to build and launch; even the legacy positional
`run` and `trace` selectors require this flag. `--race`, track/car selection and
`--debug` select launch settings and never start the game by themselves.

```powershell
.\build.ps1 --jobs 8
.\build.ps1 --run --race --track "Dark Forest" --debug
.\build.ps1 --clean --jobs 8
python scripts/build.py core --clean
```

Existing CMake/Ninja configurations, compiled objects and fetched dependencies are
reused. The script reconfigures when preset, dependency lock or Godot build-profile
content changes; Ninja handles source/header and CMake-file dependencies. This
avoids regenerating godot-cpp's trimmed API on every invocation, which previously
made every generated binding dirty. `--clean` asks CMake to clean build targets
before rebuilding; it retains configuration, dependencies and original assets.
Builds through this script are serialized per preset. Before reusing objects it
also checks Ninja's header-dependency records and rebuilds any object with an
empty record; a build that still has incomplete records fails explicitly. Use the
script for concurrent build requests rather than running direct CMake/Ninja
builds against the same directory simultaneously.

`deps` downloads godot-cpp at the immutable commit and archive checksum in `scripts/dependencies.lock.json`. That commit supports multiple API versions; the build explicitly selects the included **4.7** API for the 4.7.2 engine. Missing or mismatched dependencies fail with an actionable error. For a different CMake generator, configure a separate build directory rather than changing an existing cache.

The explicit diagnostic triangle needs no original installation. Frontend and free-driving require `game_dir` in the same local configuration. Optional executable and Ghidra paths are reserved for analysis tools. Do not commit machine paths or assets.

```powershell
python scripts/build.py --run
python scripts/smoke_godot.py --headless
python scripts/smoke_godot.py
```

The project under `platform/godot/project` uses Forward+ with an explicit Vulkan driver on Windows. The smoke test first imports the extension, then tests ten scene restarts and RID cleanup; the non-headless run also requires Vulkan Forward+, verifies colored triangle pixels and writes `build/godot_smoke.png`. Headless success alone does not prove pixels rendered. Missing tools or graphics capability are errors, not successful validation. See `docs/agent/evidence/vulkan_migration.md` for the migration checks.

## Inventory and progress

Live gameplay runs at a fixed60Hz simulation rate, independently of rendering FPS.
Body/wheel/world transforms and the camera render with interpolation. Stalls retain
catch-up time; pause/time scale remain controlled by Godot. This is an explicit
modernization of the original variable-frame scheduling. Validate with:

```powershell
python scripts/smoke_race.py --frame-independent
python scripts/smoke_race.py --timing
```

The current registry contains 9,295 functions (8,603 internal functions and 692 imports) and 132 confirmed types (121 RTTI plus 11 additional Ghidra Class symbols) from the hash-bound RoadWars Kopio program. External entries use PE IAT virtual addresses, not callable function bodies. Most business ownership and original source paths remain unknown. See the export provenance and limitations in [the inventory guide](docs/agent/progress_tracking.md).

```powershell
python scripts/import_inventory.py docs/agent/evidence/roadwars_inventory.json
python scripts/progress.py update
python scripts/progress.py check
python scripts/check_boundaries.py
```

Per-function updates use the complete stable ID shown in the CSV:

```text
python scripts/progress.py set <id> --status analyzing --evidence <reference>
python scripts/progress.py set <id> --status verified --evidence <behavior-test-reference>
```

Add `// RE: <id>` lines to mapped C++ source. Markers update source paths; they never advance completion status. Maintain classification, recovered names, source symbols, confidence and notes in the CSV. Regenerate Progress.md after edits. Its full tables are collapsed by default.

## Working rules

Agents start at [AGENT.md](AGENT.md). Read the [C++ standards](docs/agent/cpp_standards.md) before code changes. Preserve the original algorithms; do not substitute Godot vehicle physics or label approximations as recovered behavior. Original executables, assets, extracted data, dependencies and build outputs stay outside version control.

## Standalone Release package

Install the Godot export templates matching the dependency lock, then run:

```text
python scripts/package.py
```

`--templates-dir` selects a nonstandard template directory and `--godot` selects
the editor tool. Windows output is `build/bin/windows/RoadWars-RE.exe` plus
`roadwars_godot.dll`; the project pack is embedded. Copy both files together.
Place them in the original installation or a descendant such as `bin`, or select
the game directory using Browse. Selections are not saved. No original assets or
developer configuration are shipped. See [startup and distribution details](docs/agent/standalone_distribution.md)
for command-line/debug behavior, engine CWD semantics and platform validation.
