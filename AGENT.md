# Mandatory agent instructions

Read in order:

1. `docs/agent/architecture.md`
2. `docs/agent/cpp_standards.md`
3. `docs/agent/reverse_engineering.md`
4. `docs/agent/progress_tracking.md`, `docs/Progress.md`, and relevant rows in `docs/functions.csv`

## Non-negotiable boundaries

- The user explicitly authorizes fixing original frame-rate dependence. Live gameplay must use the portable `SimulationClock` fixed-step policy; rendering cadence must never determine physics callback count. Keep the legacy variable-step policy only for reference analysis/tests. Preserve accumulated elapsed time, keep interpolation visual-only, and run the cross-frame-rate regression when changing simulation scheduling or physics. Read `docs/agent/frame_independence.md`.

- Recover original behavior in portable C++; all Godot integration belongs in `platform/godot/` and uses RenderingServer for the game world.
- Organize recovered `game_src` files using original source paths, namespaces and module/class ownership **when supported by evidence**. Record that evidence before moving files. Do not copy installation asset paths as if they were original C++ paths. Keep unresolved ownership in `game_src/unresolved/`; do not invent a subsystem layout.
- `game_src/core` and `game_src/interfaces` are new infrastructure, not claims about the original source project. Never add RE markers to unrelated infrastructure to inflate progress.
- Do not bring code or findings from sibling projects into this independent reconstruction.
- Bind every reverse-engineering claim to the executable hash and addresses. Distinguish confirmed observations, high-confidence inferences and hypotheses. Ghidra decompiler output is not authoritative compilable source.
- Do not replace unknown algorithms with plausible behavior, default Godot vehicle physics, fake success or unlabeled stubs. Report unavailable behavior explicitly.
- Generated class scaffolds are explicitly authorized: use `RE-STUB` markers and `not_implemented` fail-fast tracing. They must never count as implemented. Follow `docs/agent/class_generation.md`; preserve hand-edited implementations when regenerating.
- The reconstruction goal is to implement and verify every required original gameplay class member; class generation is only the starting inventory. Keep library/import/thunk scope distinct from required game behavior.
- Preserve binary identity and human progress when importing. Never mark a function implemented merely because a source marker exists. Verified requires explicit behavior-test evidence.
- All documentation, comments and identifiers must be English. Do not commit original binaries, assets, extraction output or absolute machine paths.

## Required completion checks

Update evidence, CSV source mappings and generated progress for relevant changes. Run `python scripts/progress.py update`, `python scripts/progress.py check`, and `python scripts/check_boundaries.py`. Build the portable core and run its tests. If touching integration, build the Godot extension and run lifecycle/render smoke tests as applicable. Report precisely which checks ran and any missing prerequisites; never describe unexecuted checks as passing.

Read `docs/agent/test_driving.md` before extending the original-asset test profile. Shared initialization, vehicle force helpers and ground/wall collision now have partial portable reconstructions. Full initialization, race dispatch, missing-ground recovery and most class ownership remain incomplete. The default Godot launch runs the portable frontend; `--diagnostic` selects the triangle, and trace mode explicitly selects a limited free-driving profile. Read `docs/agent/evidence/menu_frontend.md` for screen coverage and remaining presentation limits. Keep the single installation root in ignored `config.local.json`; never add source-level machine-path fallbacks.
