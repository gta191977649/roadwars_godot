# Shared race callback cursor

Reference executable SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static decompilation and assembly are retained in `race_callback_registry_ghidra.json`.
Original native class and source names remain unresolved. No original executable was run.

`004d724f`, `004d727e`, and `004d72a9` share manager+1c and the same mutable
list cursor+c. `004edfc0` starts every search at the head and leaves a failed
search at the tail (null for an empty list). `004edc20` appends without changing
that cursor. Duplicate registration and unsuccessful removal still perform the
search. `004eded0` removes a matched node and moves the cursor to its predecessor,
or its successor when removing the head. `004ede50` advances only when a successor
exists; it leaves a tail or null cursor unchanged.

`004d72a9` captures the list count once, resets the cursor to the head, and calls
the current entry followed by Next for that many iterations. Registration and
removal inside a callback can therefore skip entries or repeat the tail. Given
A,B,C, registering D from A produces A,D,D; removing A produces A,C,C.
New callbacks cannot be deferred uniformly to the next pass. The portable
registry reproduces these cursor mutations; a native null dereference boundary
returns unavailable rather than inventing a replacement callback.

The portable race binds vehicle mounts, weapon controllers, projectiles, rear
instances, impacts, Vulcan secondary sprites, rear explosions, nitrous, collision
sparks, scene Electron traps, and weather into this one registry. Track turret
controllers and chimney effects are direct world-phase updates in `004ba95f`,
not global callback entries. The active camera updates before the global pass.
The original countdown >=3 branch gives the global pass twice the race step.

`race_callback_registry_test` checks ordinary ordering, append during dispatch,
head removal, duplicate insertion, missing removal, and null-cursor failure.
The race integration regression additionally compares actual equipment, health,
particles, actors, route state, and shared CRT state across rendering cadences.

Startup order is retained separately in `race_callback_startup_ghidra.json`.
`0048355e` resets weather before its per-vehicle loop. `004cfdf4` removes a
mount callback while resetting the mount; `004cfe98` registers that callback
only on an accepted request. Fresh mounted equipment has no live callback.
Consequently weather is the sole initial global callback in the fresh race
profile, even though the portable diagnostic restart reconstructs vehicle
coordinators before attaching the weather observer. This does not claim native
in-place restart ABI or startup wall-clock seeding equivalence.
