# Vehicle equipment, damage and nitrous effects

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only static Ghidra records are retained in
`vehicle_equipment_effects_ghidra.json` and
`vehicle_equipment_effects_supplement_ghidra.json`. Numeric/string constants were independently
read from the matching executable's PE sections. The original executable was not run.

## Confirmed observations

- `0040fd85` maps AIR FILTER, CAM SHAFT, CYLINDER HEADS, EXHAUST, INTAKE MANIFOLD,
  SUPER CHARGER and NITROUS BOOST to seven vehicle drivetrain slots.
- `00410232` uses integer base torque additions 2, 6, 10, 5, 4, 15 and 0. Contrary to
  a tempting reading of its incomplete decompiler output, these are POOR additions.
  Assembly `00410478` doubles the base for BEST; `004104a9` multiplies by float 1.5
  for GOOD and calls `__ftol`, truncating fractional results. Nitrous instead loads
  PRICE/DEFAULTAMMO from its selected quality's Data.ini.
- `0040d4cb` reads four directional HP values and INTERNALHP from the vehicle's
  DAMAGE section. Armor-array order is front, right, rear, left. It copies the
  initial values to base and maximum fields. `00410853` applies selected armor's
  HP/HPSCALE: maximum = base * scale + base. The existing current/maximum ratio is
  first stored to a float and then multiplied by the new maximum; negative armor
  values are not implicitly repaired.
- `00414e7d` rejects route-invulnerable and dying vehicles, subtracts armor damage
  without a zero clamp, and scales front damage by .5 for Battering Spikes or .25
  for Hydraulic Ram. `00414d2a` additionally requires local network authority and
  nonzero internal HP; it subtracts internal damage and clamps to zero, then calls
  the existing drive-force recomputation `0042bbf3`.
- Machine Gun contact handler `0045b9b0` maps collision sides 0,1,2,3 to armor
  indices 0,2,3,1. If that armor is already <=0, the hit damages internal HP.
  Otherwise the hit damages armor only; excess does not spill into internal HP
  during that hit. The portable generic contact helper recovers this routing
  subset; this does not establish identical hit behavior for every weapon.
- `0040d2b0` reads character PRICE_SCALARS/NITRO to vehicle+43f8. Nitrous object
  constructor `00473d30` initializes timer and ammo to zero. `00473e8e` activates
  only when idle and ammo is nonzero, sets the float timer to .01, and consumes one
  charge. `00473fa6` normalizes the full three-dimensional vehicle forward vector
  and adds forward * (12 * dt) to velocity while timer < 3 * character scalar.
  Timer increments after the impulse. Timer equal to duration does not emit and
  is not stopped until a subsequent increment makes it greater than duration.
  Stopping also requires the original particle pool to have no live particles.

## Portable ownership and integration

`VehicleEquipmentEffects` is a portable representation, not a recovered original
C++ class name. Vehicle-owned HP/upgrades and the separately allocated nitrous
object's state are kept explicit. Callers copy `engine_upgrades` to the existing
vehicle force parameters and supply the current internal/max HP ratio to drive
force. Callers supply route immunity, dying state and local authority from actual
race state. Damage helpers do not invent a destroyed-car lifecycle.

Nitrous activation must additionally honor the race weapon lock and dying-state
gates from `004246c0`. `advance_vehicle_nitrous` returns an impulse and emission
state; presentation/particle ownership remains with the existing portable effect
and RenderingServer adapters. Its `live_particles` argument must reflect that
pool, not an invented lifetime. The game must not report full nitrous visual
equivalence without restoring the original exhaust pool.

The loader reads original resource-relative paths, validates external values and
replaces output only on success. Invalid categories/quality levels are errors.
Shop prices, refunds, localized strings, original network notifications, sound,
damage-texture deformation, destruction/respawn, repair purchases and character
automatic internal repair remain outside these helpers. The original containing
functions remain partial/analyzing, even when the arithmetic tests pass.

## Validation

The warning-enabled standalone C++20 `vehicle_equipment_effects_test` passes
original formula reference cases for all quality scales including GOOD exhaust
truncation, armor ratio preservation, collision-side routing, negative armor,
front-weapon damage reduction, route/dying/authority gates, internal zero clamp,
nitrous activation/consumption, duration equality, particle-drain stop and invalid
inputs. Synthetic ResourceReader fixtures verify actual configuration loading,
selected ammo/armor/upgrades and atomic failure. These are reconstruction contract
tests, not original-executable behavioral equivalence.
