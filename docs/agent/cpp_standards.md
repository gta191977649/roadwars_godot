# C++ code standards

## Language and naming

- Use C++20 and namespace `roadwars`; filenames, functions and variables use `snake_case`, types use `PascalCase`, constants use `kPascalCase`. The required `GDCLASS`, `_bind_methods` and engine callback signatures retain their API spellings.
- Format with the root `.clang-format`: four spaces and 100 columns. Use explicit, minimal includes and `#pragma once`. All comments and documentation are English.
- Keep recovered files organized by evidenced original source/module/class ownership. Unconfirmed ownership belongs in `game_src/unresolved`; do not disguise a guessed hierarchy as recovered architecture.

## Ownership and error handling

- Use RAII, value semantics and explicit ownership. Prefer `std::unique_ptr` for unique dynamic ownership; raw pointers and spans are borrowed. Disable copying of objects owning renderer or runtime state.
- Use `std::span`, sized containers, `std::string_view` and fixed-width integer types at binary boundaries. Never cast a 32-bit original process address into a host pointer.
- Return explicit failure for malformed files, missing resources and unavailable behavior. Validate counts, arithmetic overflow and read boundaries before allocation or indexing. Assertions are for internal invariants, not external input validation.
- Catch exceptions before crossing GDExtension entry points or callbacks. Do not return success after a failed or missing implementation. Destructors must not throw.
- Rendering adapter owns RIDs; destroy instances before their meshes and release all RIDs before renderer/server shutdown. Never let Godot types escape into the portable core.

## Binary and behavioral fidelity

- Authorized modernization: live gameplay uses a fixed simulation clock and interpolated presentation. Original frame-rate dependence must not be reproduced. Keep recovered per-step formulas separate from the modern scheduling policy; never feed render-interpolated positions back into physics. Preserve a legacy reference mode for analysis, clearly labeled. Input-edge commands must survive frames without physics ticks and execute once during catch-up.

- Separate serialized structures from runtime objects. Decode little-endian fields explicitly; do not read raw bytes directly into C++ structs or depend on native padding, alignment or pointer width.
- Preserve evidenced original units, numerical widths, floating-point order and update order. No fast-math, implicit timing model changes, speculative parameter tuning or silent bug fixes.
- Do not translate decompiled pseudocode mechanically: repair calling conventions, object layouts, signedness and ownership using disassembly and corroborating evidence.
- Keep confirmed behavior distinct from hypotheses and new infrastructure. Do not substitute Godot vehicle physics for the original solver. Unknown logic remains explicitly unavailable.
- Add full `// RE: <function-id>` lines only to actual mapped implementation files. For a merged implementation, add one line per original function. Record precise `source_symbol` and evidence in the registry. Source markers do not imply completion.
- Authorized class/loop scaffolds use `RE-STUB` and the fail-fast `not_implemented` helper. A stub's signature is only a trace entry point until its portable ABI has been recovered. Never relabel a stub as implemented to satisfy progress validation.

## Validation

Use meaningful contract, file-format boundary or reference-behavior tests. Compare recovered logic against fixed inputs and original observations when available. A successful compile or triangle rendering is not behavioral equivalence. Update evidence, mappings and progress, then run the checks required by AGENT.md.
