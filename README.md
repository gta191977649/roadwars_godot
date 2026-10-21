# RoadWars: portable logic with Godot rendering

Reconstruct the original RoadWars behavior in platform-independent C++, with Godot 4.7.2 providing the host and RenderingServer rendering through GDExtension. An explicit original-asset free-driving test profile now loads Dark Forest and Animal. This is an early reconstruction, not a complete game port.

## Layout and direction

- `game_src/`: portable reconstructed logic. Recovered files follow evidenced original source/module/class organization where available. `core/` and `interfaces/` currently contain explicitly new host infrastructure. `unresolved/` is reserved for recovered code whose original location is not established; do not invent gameplay module hierarchies.
- `platform/godot/`: all Godot code, project, RenderingServer adapter and pinned godot-cpp dependency.
- `docs/agent/`: agent instructions, architecture, reverse-engineering evidence and standards.
- [Progress](docs/Progress.md) and [registry](docs/functions.csv): all known original functions and types, with separately tracked scope, implementation and verification.
- `scripts/`: build, validation, inventory and progress automation. `tests/`: portable contract and tracking tests.

The original executable is the behavioral reference. Recovered vehicle force helpers and original ground/wall collision run in portable C++; no Godot physics is used. AI and race rules remain unavailable. New infrastructure is not counted as recovered functions. No neighboring RoadWars project has been imported. Default launch still supplies the triangle; trace mode selects the free-driving profile. Full initialization and race dispatch remain partial.

## Drive the test scene on Windows

Copy `config.example.json` to ignored `config.local.json` and set `game_dir` to the original installation containing Data.pak/Data.pkh, and `godot_executable` to Godot 4.7.2. This is the single location defining the local game root.

```powershell
python scripts/build.py deps
.\build.ps1 --debug trace "Dark Forest"
.\build.ps1 --jobs 8 --debug trace "Dark Forest"
```

`--jobs N` sets CMake build parallelism for `core`, `godot` and `trace` (default4).
Use a positive integer; the option can appear before or after the action.

`trace` takes a track name, quoted when it contains spaces. W/Up accelerates, S/Down brakes then reverses, A/D or Left/Right steers, Space applies full braking, R resets to Start1, and Escape exits. Move the mouse to orbit; after three idle seconds it gradually returns to the recovered chase camera. The undamaged profile uses stock Street/Poor tires and original automatic shifting when MANUALTRANSMISSION is disabled. Original torque, gear/RPM and grip formulas run in portable C++; no extra acceleration slowdown is applied. Original engine PCM loops through Godot audio. Original texture/alpha rules, sky, regional view distance, linear vertex fog, wheels, sparks, tire smoke/dust and skid strips use native Godot rendering. AI, weapons, damage, laps and original menus remain incomplete; missing-ground respawn still traces/asserts. Mouse orbit and reset are new conveniences. See [profile coverage](docs/agent/test_driving.md) and [rendering audit](docs/agent/rendering_corrections.md).

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
python scripts/build.py run --original-loop
```

Generated output currently covers 132 confirmed types and their 262 associated functions. Original source directories remain unconfirmed, so files use `game_src/unresolved/types`. Most known RTTI types are library types; unassigned gameplay functions remain unresolved. Each stub prints original identity and source location, asserts, and aborts even in release mode. Strict launch intentionally stops at the first unimplemented original-loop stage. Default launch still shows the diagnostic triangle.

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

`deps` downloads godot-cpp at the immutable commit and archive checksum in `scripts/dependencies.lock.json`. That commit supports multiple API versions; the build explicitly selects the included **4.7** API for the 4.7.2 engine. Missing or mismatched dependencies fail with an actionable error. For a different CMake generator, configure a separate build directory rather than changing an existing cache.

The diagnostic triangle needs no original installation. Free-driving requires `game_dir` in the same local configuration. Optional executable and Ghidra paths are reserved for analysis tools. Do not commit machine paths or assets.

```powershell
python scripts/build.py run
python scripts/smoke_godot.py --headless
python scripts/smoke_godot.py
```

The project under `platform/godot/project` uses the Compatibility backend. Escape exits. The smoke test first imports the extension, then tests ten scene restarts and RID cleanup; the non-headless run also verifies colored triangle pixels and writes `build/godot_smoke.png`. Headless success alone does not prove pixels rendered. Missing tools or graphics capability are errors, not successful validation.

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
