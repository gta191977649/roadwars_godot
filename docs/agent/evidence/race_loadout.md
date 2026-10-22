# Single-race equipment selection and tire plumbing

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static Ghidra decompilation and assembly are retained in `race_loadout_ghidra.json`.
No original executable was run. Portable value types do not establish original class names.

## Confirmed observations

- `004a4f79` loads the six vehicles, calls `004a55a9`, then initializes the race
  through `00484189`. Configuration selection is not part of the rendering host.
- `00404490` reads each weapon slot's ORDERING table and filters by PRICE/ISAVAILABLE,
  defaulting availability to one. Prices are read by the original helper, but the
  portable selection view does not implement purchases or expose price output.
- `004a55a9` selects paint through `0041c3d9` (`rand()%6+1`), reads track
  World/Data.txt AI/TIRES, and installs that tire type at BEST quality. Tires are
  not randomly selected. The three weapon slots each consume one selection draw.
  Armor consumes a category draw and a quality draw. Every drivetrain category
  consumes one quality draw, including nitrous; NITROUS BOOST is then set to BEST.
- Assembly `004a5b42..004a5b7d` restores the weapon name missing from the decompiler;
  armor similarly uses two distinct indices. The apparent tautological modulo
  predicates are real assembly and do not describe a probability of equipment absence.
- `004a597d..004a59c3` always configures opponents. Player configuration is conditional
  on the caller flag or race+eec. The fresh diagnostic race explicitly uses the new
  vehicle/configuration branch, without a preserved garage vehicle.
- `00412834` uses the selected tire type and quality for mesh, X scale, tire width,
  slip angles, and added friction. `0042b31a` compares tire-kind to surface-kind;
  its reduced mismatch penalty applies only to Street tires on kind2 ground.

## Portable integration and limits

`RaceLoadoutTable` is an inferred portable view under unresolved. It retains the
ordering tables and selected configuration. `VehicleDynamics` owns the effects
state. Player/opponent geometry and RenderingServer presentation receive the
same TireSelection; no competition tire path remains fixed to Street/Poor.
The trace/free-driving profile still explicitly selects stock Street/Poor.

All character selection draws precede equipment assignment. The shared RNG uses
the confirmed CRT recurrence. Constructor-driven RNG ordering is audited in `race_startup_rng.md`; live setup must
use synchronous installers to interleave Reactive Mines initialization before armor
and upgrade draws. Original startup seeding and preserved garage repaint restoration
remain incomplete, so the containing setup function remains partial. The selected paint index is recorded but original
paint texture replacement is not yet integrated. Weapon names are selections,
not a claim that all weapon objects have been instantiated or can fire.
The debug snapshot explicitly reports incomplete combat dispatch.
The original keyboard weapon keys are decoded through the existing PSE-first
configuration path and sampled by a Godot-only DirectInput scan-code adapter.
The simulation clock latches weapon/nitrous/current-item/cycle edges across
zero-tick frames. Live weapon requests fail explicitly while the complete race
combat dispatcher is unavailable; they never silently consume ammunition.
AI behavior and engine torque now read the actual internal/maximum HP fields.
Diagnostic restart restores the loaded equipment snapshot; full original reset
side effects and RNG interleaving are not asserted recovered.

## Validation

`race_loadout_test` checks the CRT reference sequence, availability filtering,
assembly-derived selection order, track-selected BEST tires, nitrous override,
invalid tables, and malformed availability. Its optional installation argument
loads six selections for Dark Forest and RWA Speedway and loads their actual tire
geometry/scales. Dark Forest selects STREET/BEST; RWA Speedway selects SLICKS/BEST.
`race_session_test` serializes RNG/equipment state for frame-rate/debug-read checks
and checks that selected tires, physics upgrades and debug snapshots agree.
These are reconstructed execution tests, not original-executable equivalence.
