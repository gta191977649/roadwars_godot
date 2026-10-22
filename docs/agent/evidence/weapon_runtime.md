# Weapon factory, mount and scene turret evidence

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra resource decompilations, selected assembly listings and
hash-matched PE vtable/constants reads are retained in `weapon_runtime_ghidra.json`.
The original executable was not run. Tests run the reconstructed code.

## Object ownership and identity

Vehicle-owned factory `00410ea2` constructs the following 21 configured names.
Each constructor installs the listed vtable and writes its name at object+110.
This confirms distinct implementations and shared inheritance relationships; it
does **not** recover their original C++ class names or source filenames.
`WeaponDescriptor`, `WeaponMountRuntime`, `TurretRuntime` and `RaceTurrets` are
explicit portable names under unresolved. No class IDs are invented.

| Configured name | Slot | Constructor | Initializer | Vtable |
| --- | --- | --- | --- | --- |
| Smoke Screen | Rear | 004a6110 | 004a61ec | 005f5960 |
| Oil Slick | Rear | 00474270 | 004743bb | 005f57ac |
| Missile Jammer | Rear | 00464f50 | 00465038 | 005f573c |
| Grenades | Rear | 0044db70 | 0044dc6a | 005f5534 |
| Reactive Mines | Rear | 0049ce70 | 0049cf6a | 005f5894 |
| Arachnoids | Rear | 00406320 | 0040641a | 005f5024 |
| Land Mines | Rear | 004563d0 | 004564ca | 005f5660 |
| Machine Gun | Front | 0045ae10 | 0045af2e | 005f56d0 |
| 20mm Cannon | Front | 0040c300 | 0040c41e | 005f5178 |
| Electron Cannon | Front | 00433710 | 00433867 | 005f53b4 |
| Plasma Blaster | Front | 0047d000 | 0047d11e | 005f582c |
| Battering Spikes | Front | 00407ec0 | 00407f9c | 005f50b8 |
| Hydraulic Ram | Front | 00455bc0 | 00407f9c | 005f560c |
| Vulcanized Gun | Front | 004ce560 | 004ce67e | 005f59e0 |
| Flame Thrower | Front | 004352f0 | 004353f0 | 005f5460 |
| Rocket Launcher | Side | 0049f330 | 0049f460 | 005f590c |
| Gatling Gun | Side | 0044c720 | 0044c83e | 005f54d8 |
| Guided Missile | Side | 0044e820 | 0044e926 | 005f5594 |
| Back Missile | Side | 0044e820 | 0044e926 | 005f5594 |
| Swarmers | Side | 0044e820 | 0044e926 | 005f5594 |
| The Excalibur | Side | 00434b90 | 0044e926 | 005f5408 |

`004cf29a` is their common parameter initializer. It reads DAMAGE/HITPOINTS and
PHYSICS/AMMO_COUNT, AMMO_TOP_SPEED, AMMO_ACC, AMMO_IMPACT_MULTIPLIER,
AMMO_YROTATIONAL_SPEED, AMMO_MASS_IN_POUNDS and AMMO_TRACKING_TIMER_SECONDS.
The mass multiplier at005f51f8 is float0.4535999894142151.
`004d0778` reads PRICE/DEFAULTAMMO. The portable parameter loader validates all
these reads and retains PSE-first resource lookup. Installed configurations for
all21 factory branches passed the optional original-asset parameter test.

`004cf8d4`, the base virtual+28 entry, is a configured-name classifier. It returns
true for Guided Missile, Back Missile, Swarmers and The Excalibur; it is **not**
an active-state query. Weapon activity is object+270.

The constructor/initializer functions inspected here contain no direct rand
calls. This bounded observation does not establish the random behavior of all
renderer, effect or resource-loading descendants.

## Mount reservation and deployment

Assembly0040fb64..0040fb70 confirms that `0040face` always constructs side mount
state at vehicle+4e24 while searching ULRORMount1..3 in ascending order. The last
existing exact node therefore wins. It does not create three independently
firing side weapons. FrontWeaponMount constructs state at+4de4; RearWeaponMount
at+4e64. The +1c weapon pointers yield vehicle slots+4e00/+4e40/+4e80.

`004cfe98` requires inactive mount+30, nonzero ammunition+3c and virtual+2c
readiness. It reserves ammunition immediately, stores the supplied firing speed
at+24 and activates its callback. Subsequent reservations cannot pass while
the mount is pending. Cancellation does not refund ammunition.

`004d0725` dispatches mount type0 to004d0574, type1 to004d03d6 and type2 to004d060d.
Front emits the configured heading; rear adds180 without normalizing it. Side
mounts rotate at180degrees/sec to-90, set their deployed latch and emit the firing
call on the next callback. Once the weapon reports idle, they retract to0 and
hide. Portable mount code emits typed firing arguments; emission does not claim
that a projectile was simulated, a target was hit or damage was applied.
Native attachment matrices, child reparenting, replay records and callback list
ownership remain separate from these state/argument helpers.

## Scene turrets

`004b7439` loads track DATA/GUNS into+79bc, finds prefix Turret objects with
search length6 and creates0xd0-byte objects through `004c7c80`. It records them
in track+79c0/+79c4. Turrets are discovered/constructed even when GUNS is zero.
Vehicle character initializer `0040d2b0` reads PRICE_SCALARS/NOT_A_TURRET_TARGET
into vehicle+4410. `004db487` reads Ini/RoadWars.ini [ROADWARS]/DIFFICULTY_LEVEL
into global+938. Single-race callers must supply that configured difficulty.

The constructor reads name characters7..9 as initial yaw. An F/f at11 selects
unrestricted yaw and weapon code characters15..16; otherwise characters11..13
and15..17 are left/right arc limits and18..19 are the code. Codes _C/_V/_G/_P/_M
select 20mm Cannon/Vulcanized Gun/Gatling Gun/Plasma Blaster/Machine Gun. Other
codes, including an absent suffix, select Rocket Launcher. Their added speeds
are90/70/60/110/40/0 and intervals.5/.5/1/.5/1/5 seconds. Base firing speed is60.
Maximum target range is2*ammo_acceleration+2*(60+added_speed), not a guessed radius.
The constructor sets its firing timer to its interval, not zero.

`004c8fde` selects the nearest participant strictly inside that 3D range, preserving
participant order for ties. It excludes nonzero vehicle+134, `00414cb2` invulnerability
and NOT_A_TURRET_TARGET. A changed target clears five-sample velocity history.
`004c8729` scans only when its timer is strictly greater than3 seconds, discards
the timer remainder and updates aiming each simulation callback.

`004c914e` averages up to five **velocity differences**, without dividing by dt.
It searches a1.5-second interception horizon using target velocity, that average,
weapon acceleration and the configured speeds. AI targets at vehicle+144!=0 use
tolerance.25; player difficulty0/1/2/3 uses.25/.15/.09/.03. The search's target
prediction uses average_delta*t^2, but its final returned point uses
average_delta*t. That original distinction is preserved. An unreachable target
returns the turret origin, suppressing firing.

Aiming preserves the original180degrees/sec turning, yaw arc limits and pitch
limits20/340degrees. `004c959f` requires exact desired/current yaw and pitch plus
five collected samples. `004c8e7b` accumulates a separate firing timer, tests the
predicted point against origin with strict component epsilon.05 and fires only
when aligned and the interval has elapsed. This function contains no line-of-sight
query. The event speed argument is60, leaving each weapon's own added speed to
its launch implementation. Barrel offset is the turret object's own maxZ-minZ
vertex bounds, per004d414f/004d4058; children are not included.

Assembly0047c5d6..0047c5e3 preserves an exact-axis quirk: when the desired normalized
direction equals forward+Z, the heading helper returns360. From initial yaw0 at
fixed60Hz, the turning branches alternate normalized yaw0/357 while desired yaw
remains360 for the exact-alignment test. This turret does not align/fire at that
exact canonical direction. Other initial angles can take a snap branch that
assigns360 directly; this is not a universal firing prohibition for every turret.
The portable regression preserves this observed branch instead of silently
normalizing the desired angle. Off-axis targeting remains independently tested.

Axis initialization004dab4a/004dabb0/004dabd2 sets forward+Z, pitch axis-X and
yaw axis+Y; see the retained axis listings in weapon_projectiles_ghidra.json.
Positive pitch raises the muzzle. Local yaw/pitch snapshots identify the actual
world mesh node and its child-index path for presentation.

`RaceTurrets` currently supports the translated static hierarchy used by all
installed track turrets. A transformed dummy, RotObject or nested turret ancestor
fails explicitly because the current MSH decoder does not retain sufficient
full-basis state for that placement. Tests successfully loaded every installed
track; RWA Speedway contains six recovered turrets. No original-runtime targeting
or pixel equivalence is asserted.

## Validation and remaining coverage

Warning-enabled GNU C++20 standalone builds and synthetic tests passed for
weapon_runtime, turret_runtime and race_turrets. Optional tests against the
configured original installation passed all21 weapon parameter loads and all
installed track turret loads. Tests cover reservation/no-ammo/readiness, side
deployment and retraction, rear heading, malformed configuration, scan timing,
3D range/exemption gates, five-sample interception, fire cadence, muzzle offsets,
GUNS disabled, reset and unavailable transformed ancestry.

Factory inventory and mount events alone do not restore all21 effects. Individual
projectile/salvo, guided missile, beam, flame, mine, Arachnoid, oil, smoke, jammer,
spike and ram behavior must be tracked by their own implementation/tests. Broad
factory, base initialization, mount and turret functions remain partial. Native
cache/callback/replay branches and original-executable behavioral comparison are
not completed by successful portable tests.
