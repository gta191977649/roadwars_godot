# Agent documentation index

Start with the root AGENT.md. Required reading order:

For simulation changes, also read [Frame independence](frame_independence.md):
the user-authorized fixed clock, input edges, interpolation and required regressions.

1. [Architecture](architecture.md): portable boundaries, RenderingServer ownership and source organization.
2. [C++ standards](cpp_standards.md): mandatory coding and behavior-preservation rules.
3. [Reverse-engineering workflow](reverse_engineering.md): executable identity, evidence and unknowns.
4. [Progress tracking](progress_tracking.md): complete inventory, explicit status and source markers.
5. [Progress dashboard](../Progress.md) and [CSV registry](../functions.csv).
6. [Class generation and entry loop](class_generation.md): generated stubs, safe regeneration, evidence-backed scheduling and strict trace mode.
7. [Original-asset free-driving profile](test_driving.md): shared initialization, local game root, recovered driving and collision, launch and coverage limits.
8. [Rendering corrections](rendering_corrections.md): coordinate handedness, original textures/material states, native Godot sky/fog/lighting/particles, and verification limits.

The [initial assessment](REVERSE_ENGINEERING_ASSESSMENT.md) records static observations and limits. `evidence/roadwars_inventory.json` is the current Ghidra/PE inventory snapshot. `evidence/bootstrap_validation.md` records infrastructure acceptance, not game behavior verification.
