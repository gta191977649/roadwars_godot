# Entry-loop and class-generator validation

Date: 2026-10-22. This extends bootstrap validation and does not claim original gameplay is implemented.

## Evidence

- Ghidra decompilation confirms CRT entry 00519480 calling game entry 0045c29c. The original top-level loop and relevant callees are retained in entry_loop_ghidra.json, bound to the Kopio executable SHA256.
- Entry-loop constants were read from the same executable: little-endian float at 005f50b0 is 0.10000000149011612, and float at 005f50b4 is 1000.0.
- Ghidra symbols.get confirmed 11 additional symbols explicitly typed Class outside the 121 RTTI types. Their retained symbols are in class_namespace_symbols.json. NO ADDRESS symbols use stable, documented member-address anchors; no gameplay class or original source directory was guessed.
- Current generated coverage is 132 types, 262 associated functions, 264 header/source files plus the CMake source list. Unassigned function ownership remains unresolved. Generated methods are portable trace stubs, not claimed original signatures, memory layouts or implementations.

## Validation

- All generated source files compile with the portable core.
- Portable CTest: 7/7 passed (runtime, entry scheduler, NDEBUG fail-fast trace, Python suites, boundaries, progress, generated-file consistency).
- Python suite: nine tests cover generation idempotence, preserved edits, new-template conflicts, evidence-required paths, traversal rejection, stub-only completion rejection and previous tracking behaviors.
- Entry contract tests cover startup/failure, stage order, optional joystick/inactivity hooks, three mode branches, multiplayer transitions, quit propagation, variable delta clamp and invalid inputs.
- The release-mode subprocess test checks stderr identity/source location and nonzero termination even with assertions disabled. The debugger breakpoint is roadwars::not_implemented.
- The Godot diagnostic integration renders the triangle and survives ten restarts with six owned RIDs while running and zero after exit. It uses GameEntry.advance through the Godot frame callback.

## Boundaries

Strict original-loop mode is intentionally incomplete and stops at initialization 0045c8ab; no claim is made that the original initialization or gameplay runs. Diagnostic stage hooks are new infrastructure, not recovered behavior. Exact original Win32 absolute-clock precision, FPS/modal-loop behavior and optional-state predicates remain unported. The process-level abort contract is tested independently in NDEBUG mode; the interactive debug assertion dialog is not used as an automated acceptance mechanism.

Original C++ directory paths remain unconfirmed. New directories require corroborated evidence; resource-installation paths are not source paths. Library stubs are references and do not convert excluded dependencies into required gameplay work. No original executable was run or modified.
