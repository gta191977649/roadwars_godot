# Single Race startup RNG sequencing audit

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static Ghidra evidence: `race_startup_rng_ghidra.json`, supplemented by existing
weapon/trail initializer ledgers. No original executable was run. This audit covers
the fresh Single Race path. Diagnostic AI counts below5 remain a deliberate extension
of the original six-slot setup and must not be described as an original lobby mode.
The diagnostic seed still does not claim recovery of original startup seeding.

## Confirmed original order

1. `004a527a` prepares six character slots, excludes the selected player from the
   available opponent table and completes five rand()%remaining choices. Each selected
   candidate is removed, shifting the remaining table. Assignment fills first empty
   slots in index order. All character draws finish before vehicle construction.
2. `004a4f79` iterates all six slots. It creates and initializes each vehicle except
   an existing selected-player vehicle. `0040d4cb` constructs four wheel particle
   systems using `004e8306` -> `00506485`; each consumes trunc(20*effectsScalar)
   atlas draws. This occurs at vehicle construction, not tire installation. The
   vehicle constructor and generic spark/Nitrous configuration do not add draws.
   `0040d4cb` installs stock Street/Poor; the loader invokes `00412834` again.
   `00412834` changes geometry/physical configuration and consumes no RNG itself.
3. `004a55a9` iterates all six configuration slots. Opponents are always configured.
   The player is configured only when the supplied force flag is nonzero or race+eec
   is nonzero. Skipping this branch skips paint, tire, weapon, armor and drivetrain
   draws and preserves the existing configuration.
4. Each configured slot selects paint rand()%6+1 and applies it immediately; reads
   track AI/TIRES and installs BEST; then repeats **select one slot and immediately
   construct/install that weapon** in Front,Side,Rear order. Only after those object
   initializers return does it select armor category/quality. It selects and applies
   each drivetrain quality in table order, including a random Nitrous quality,
   then explicitly installs Nitrous BEST with no extra selection draw.
5. `00484189` constructs the track after all loadouts. Its observed snow constructor
   `004d0bf0` initializes a deterministic grid, not random positions; its reset
   `004d10a9` attaches callbacks without drawing. Scene turret weapon constructors
   reuse the same audited common weapon initializers.
6. `0048355e` resets active participants in order. Each consumes four draws: side
   delay rand()%30+constant; timer14 rand()%500/1000; timer10 rand()%500/1000;
   rear timer rand()%1500/1000. These draws apply to the human participant too.
   The reset does not reconstruct Reactive particle arrays or draw their initial
   UVs again. Callback scheduling and later gameplay consumption are separate.

## Initializer RNG audit

| Owner/initializer | Initialization draws |
| --- | --- |
| Each vehicle's four tire effects004e8306/00506485 |4*trunc(20*effectsScalar)|
| Generic ParticleEmitter constructor004e5d50 and setup004e5e99 |0; randomization occurs at spawn|
| Flame100-slot initialization004ef747 |0; spawn/update consumes later|
| Rear primary explosion004f4fdb |0; creation/update consumes later|
| Rocket specialized smoke004f9037 |0; first emission consumes later|
| Nitrous00473d30 |0; generic emitter setup|
| Reactive extra emitter004f641f |10 per instance|
| Reactive weapon0049cf6a |5 instances with extra flag1, hence50 before armor draw|
| Land Mines004564ca |5 instances with extra flag0, hence0|
| Arachnoids0040641a/0040667f |5 instances forwarding extra flag0, hence0|
| Grenades0044dc6a |5 primary emitters, hence0|
| Other17 weapon setup paths |No eager RNG in the recovered constructor/setup paths|

The weapon factory invokes virtual instance initializers. A direct-call graph alone
does not prove absence of RNG; Reactive's virtual dispatch was explicitly resolved
to004567fa with flag1, while Land Mines/Arachnoids pass0. Generic emitter initializers
do not pre-randomize full capacity arrays. Consuming guessed100 Flame or generic
particle draws at construction would be wrong.

Fresh paint replacement0041c3d9 ->00411ecc ->00420935 has no extra random draws:
retained damage/decal counts start0 in0041f8a5. Preserved vehicles can have existing
weapons and damage marks; repaint restores those objects and can reach004224c0,
whose new-mark path consumes eight draws per mark. Thus a preserved player's forced
reconfiguration is not equivalent to creating a fresh vehicle. The portable fresh
diagnostic does not claim recovery of all saved garage/decal restoration branches.

## Implementation and checks

`select_race_characters` implements the ordered removal phase before constructors.
`original_randomizes_loadout` records the verified player predicate.
`RaceLoadoutTable::assign` has a synchronous `RaceLoadoutInstallers` overload. Paint,
tires, each weapon, armor and each drivetrain application occur at their original
positions. Callbacks can consume the caller's shared stream, and a failure aborts
setup explicitly. This avoids selecting every field first and constructing weapons
afterward. The previous overload remains an explicitly selection-only fixture API.
It does not pretend to execute constructor side effects.

`race_loadout_test` now checks six-slot character ordering/removal, preserved-player
predicate and an actual five-pool Reactive constructor callback. It verifies all50
eager draws occur before armor/drivetrain, compares final RNG state and checks
installer order including the random Nitrous application followed by BEST override.
Warning-enabled build, synthetic tests and installed Dark Forest/RWA Speedway loadout
and tire geometry profiles pass. The installed profile's legacy selection-only calls
remain table/asset checks, not integrated startup-equivalence evidence.
RaceSession integration is performed separately to respect its exclusive owner;
the helper tests do not by themselves establish full original startup equivalence.
