# Class generation and entry-loop scaffold

## Current evidence and output

`scripts/generate_classes.py` reads the canonical registry, the retained Ghidra inventory and `class_layout.json`. It generates one header/source pair for each of the 132 currently confirmed types and 262 functions with exact class associations. Many of these are MFC/D3DX/library types, not newly recovered gameplay classes. Their generated presence does not make library recreation a gameplay requirement or change existing scope exclusions. Unassigned functions are retained in the CSV and are never arbitrarily attached to a class.

The type inventory includes 121 RTTI types and 11 additional Ghidra symbols explicitly typed Class (retained in evidence/class_namespace_symbols.json). Additional class IDs use a recorded member-address anchor because their class symbol has NO ADDRESS; never re-anchor these IDs when more members are found.

There is currently no confirmed original C++ source directory for these types. Therefore generated files are under `game_src/unresolved/types`. Installation directories such as Cars and Tracks are asset paths and are not used to infer original source paths. Add future confirmed class ownership to the CSV and corroborated directory overrides to `class_layout.json`.

```text
python scripts/generate_classes.py
python scripts/progress.py update
python scripts/generate_classes.py --check
python scripts/progress.py check
```

An optional per-class layout entry uses the full stable class ID as its key and contains `directory` (repository-relative, below game_src) and `evidence` (the Ghidra source-path evidence reference). Unconfigured classes always use the unresolved fallback. The mapping is a placement policy, not proof of original source layout by itself.

## Stub contract and implementation workflow

Original signatures remain documented as observed Ghidra signatures. They often contain uncertain parameter types, Windows types or unresolved calling conventions. The compilable scaffold deliberately uses `void unresolved_<name>_<address>()` methods and address-suffixed types in `roadwars::recovered`. These are trace entry points, **not recovered ABI declarations**. Constructors/destructors are also explicit address-named methods; ordinary C++ object lifetime does not silently invoke an unknown original constructor/destructor. Inheritance, virtual slots and fields are not guessed.

Calling a stub prints NOT IMPLEMENTED, full original function ID, original class/function, source file/line and compiler function name to stderr, flushes, triggers a debug assertion and always aborts even when NDEBUG removes assertions. Break on `roadwars::not_implemented` to inspect the debugger call stack. There is no success-returning fallback.

Generated stubs use `RE-STUB`, not `RE`. Source tracking includes both for navigation, but implemented/verified status requires a genuine `RE` mapping. Before advancing status, recover the actual portable signature/body, remove the stub call and change the marker to `RE`. Record behavior evidence and source symbol. Generation itself advances no status.

`class_generation.json` records template hashes. Repeated generation is deterministic. User edits are preserved if the relevant template has not changed. If new members, names or placement require changing a hand-edited file, generation fails with a manual-merge message before rewriting files. Moved/removed outputs are reported and never deleted automatically. Resolve a class migration explicitly and update the manifest after reviewing the new layout; do not delete a manifest to force overwrites.

All emitted source files are compiled into roadwars_core using `generated_classes.cmake`. A successful scaffold compile proves only that trace entry points build; it does not establish original behavior.

## Entry and per-frame integration

Ghidra independently confirms `00519480` as CRT startup calling game entry `0045c29c`. The latter initializes through `0045c8ab`, then runs a loop with input polling, common handling, optional inactivity processing, shortcut handling, mode dispatch and multiplayer transitions. Mode values 0/1/2 correspond to single race/multiplayer/season, corroborated by menu strings in `0045d8ed` and their dispatch call sites. Decompilations are retained in `evidence/entry_loop_ghidra.json`.

`game_src/unresolved/game_entry` is a new portable host adaptation, **not an original class identity**. Godot `_ready` starts it, `_process` advances one iteration, and `_exit_tree` destroys it before runtime/render resources. There is no nested blocking while loop, Win32 message pump or competing renderer loop.

The observed loop converts elapsed milliseconds to seconds using float 1000 at `005f50b4` and clamps values above float 0.1 at `005f50b0` to 0.1. These constants were checked against the hash-matched PE bytes. The adaptation receives Godot elapsed seconds and applies that clamp. Original float absolute-clock precision, FPS text, modal screen loops, inactivity predicates, shortcut bodies, network transition state and race update algorithms are not fully ported. Optional-stage predicates are explicit host state; they are not recovered layouts.

Default launch remains an explicitly diagnostic triangle using the same scheduler and rendering lifecycle. Diagnostic hooks are not original game implementations. To enter strict original-stage tracing:

```text
python scripts/build.py run --original-loop
```

Strict mode intentionally prints the first missing stage and terminates, currently initialization at `0045c8ab`. Do not describe it as a playable original loop. Implement each stage and its classes against evidence until the original game path can advance. Stage addresses stay visible throughout this process.

Tests cover stage order, all three mode branches, optional gates, quit/failure propagation, float clamp/invalid deltas, generator idempotence, safe paths, edited-file protection and stub-only completion rejection. A subprocess test confirms release-mode fail-fast tracing. Godot smoke acceptance continues to verify diagnostic rendering and RID cleanup.
