# RoadWars: portable logic with Godot rendering

Reconstruct the original RoadWars behavior in platform-independent C++, with Godot 4.7.2 providing the host and RenderingServer rendering through GDExtension. This repository currently contains build infrastructure and a generated triangle, not a playable port.

## Layout and direction

- `game_src/`: portable reconstructed logic. Recovered files follow evidenced original source/module/class organization where available. `core/` and `interfaces/` currently contain explicitly new host infrastructure. `unresolved/` is reserved for recovered code whose original location is not established; do not invent gameplay module hierarchies.
- `platform/godot/`: all Godot code, project, RenderingServer adapter and pinned godot-cpp dependency.
- `docs/agent/`: agent instructions, architecture, reverse-engineering evidence and standards.
- [Progress](docs/Progress.md) and [registry](docs/functions.csv): all known original functions and types, with separately tracked scope, implementation and verification.
- `scripts/`: build, validation, inventory and progress automation. `tests/`: portable contract and tracking tests.

The original executable is the behavioral reference. No physics, AI, race rules, mesh loader or original gameplay is implemented yet. New infrastructure is not counted as recovered functions. No neighboring RoadWars project has been imported. Godot now advances the evidence-backed entry-loop scaffold once per frame; diagnostic mode supplies only the triangle, while strict original-loop mode stops at missing initialization.

## Class scaffolds and original entry loop

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

Copy `config.example.json` to ignored `config.local.json`; set `godot_executable` to your Godot 4.7.2 executable. The optional original-game and Ghidra paths are reserved for analysis tools; the triangle needs no original installation. Do not commit machine paths or assets.

```powershell
python scripts/build.py run
python scripts/smoke_godot.py --headless
python scripts/smoke_godot.py
```

The project under `platform/godot/project` uses the Compatibility backend. Escape exits. The smoke test first imports the extension, then tests ten scene restarts and RID cleanup; the non-headless run also verifies colored triangle pixels and writes `build/godot_smoke.png`. Headless success alone does not prove pixels rendered. Missing tools or graphics capability are errors, not successful validation.

## Inventory and progress

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
