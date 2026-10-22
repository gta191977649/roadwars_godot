# Original vehicle AI weapon predicates

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
The existing analyzed RoadWars program was inspected read-only. Static decompilation,
assembly and memory reads are retained in `race_ai_combat_ghidra.json`. The original
executable was not run. Original C++ class/source names remain unresolved.

## Confirmed ownership and dispatch

`0041a1cd` receives the same vehicle object as driving controller `0041aa67`.
It reads vehicle+4418 (track), +4420 (route node), +4494 (chassis identity),
+4e00/+4e40/+4e80 (front/side/rear weapon pointers) and vehicle-owned timers.
Weapon virtual+28 supplies a guided-kind predicate (004cf8d4); weapon+270 is the active field. Virtual+1c refreshes the
side target; weapon+08 is then a vehicle pointer. `004242cb` performs the actual
activation. These establish field ownership, not original class names or native ABI.
The portable views use participant indices and integer identities, never cast
original pointers into host pointers. Their types are new portable representations.

## Confirmed predicates and order

- Vehicle+4de0 gates the entire AI combat callback, including both timers.
- With vehicle+4aec nonzero, a present front weapon is attempted every callback;
  a present side weapon is attempted only when virtual+28 returns zero. This branch
  neither decrements +4ad4 nor calls side virtual+1c.
- With +4aec zero, a present guided-kind side weapon decrements +4ad4 by dt. At <=0 it
  refreshes its target. The target must exist, have a nonzero absolute main-node
  difference and satisfy difference <=4 or difference >= main-node-count-5.
  Assembly `0041a32d` establishes the literal4; the broken decompiler loses it.
- `0041a3ea..0041a467` gathers walls around two copies of the current vehicle world
  position with radius3 and height flag0. `0041a46c..0041a567` builds the three-unit
  heading+90 ray and tests each candidate wall through `0047c368`, stopping at the
  first hit. This is a local side wall guard, not target-to-target line of sight.
- A clear wall guard then compares the two main-route lateral vectors at node+2c
  with `0047c1b4` and acos; angle must be strictly <35. Activation of slot1 precedes
  the shared RNG draw; +4ad4 becomes rand()%15+10 even if the activation is rejected
  by normal weapon readiness checks. Failed activation caused by unavailable
  reconstruction is an explicit portable error and does not consume fabricated RNG.
- After the side branch, vehicle+0c increases by dt, then `0041a69a` runs, then
  `0041a7e7` runs. They share this exact timer.
- `0041a69a`: rear name compares case-insensitively with Missile Jammer and timer
  must be >3. It resets timer before scanning, including scans with no match.
  A candidate must have different chassis identity, vehicle+134 exactly0, a present
  guided-kind side weapon, its target equal this vehicle and weapon+270 nonzero. The first
  match activates slot2. No random draw follows.
- `0041a7e7`: rear must exist and not be Missile Jammer; timer must be >1.5. It
  resets before scanning. Reference is exactly wheel-cache+5b70 minus +5b88 plus
  current chassis position. It is not replaced with a normalized heading vector.
  A different chassis with +134 exactly0 qualifies if its 3D displacement angle
  against that reference is strictly135..215 and 3D distance strictly5..30.
  After the first slot2 activation, timer subtracts rand()%10+5.

`0047c1b4` divides the float dot product by the two vector moduli and clamps to
[-1,1]. The acos result is rounded to float before multiplying original degrees
constant57.288352966308594. Degenerate vectors yield NaN and fail comparisons;
the portable predicate does not invent a direction for them. `0052bdc0` confirms
ASCII case-insensitive name comparison; locale-specific CRT behavior is outside
these original ASCII weapon names.

## Portable integration contract and limits

`advance_race_ai_combat` owns only these vehicle-owned predicates and timers.
Its services read current self weapon fields, refresh the actual side target,
perform the exact wall guard, activate synchronously, and draw from the same RNG
as gameplay. Missing required services return unavailable instead of silently
pretending a successful shot. Synchronous activation preserves any weapon RNG
consumption before the following AI cooldown draw. The caller must supply the
original outer `0041aa67` race-time/finish gate and initialization/reset values;
the separate reset helpers implement the later recovered reset assignments below.

Actual weapon readiness, firing,
damage and guided-projectile ownership are separate runtime behavior. The wall
ray construction remains an explicit service until that integration uses recovered
runtime basis vectors. This implementation therefore establishes predicate behavior,
not a complete original AI weapon subsystem or original-executable equivalence.

## Executed validation

Standalone C++20 GNU build with `-Wall -Wextra -Wpedantic` and
`race_ai_combat_test` passed. Tests cover disabled timer freeze, direct front/side
dispatch, guided-kind side timer gating, target refresh, same-node rejection,4-node
and wrap acceptance, blocked walls, curved/degenerate route vectors, strict timer
and distance boundaries, failed-scan timer reset, jammer active/target/identity/
disabled-car guards, activation-before-RNG order and explicit unavailable services.
These are static-reference contract tests; no original-runtime comparison was run.

## Later recovered initialization, player input and missile target selection

Additional bodies and assembly are retained in `race_ai_combat_setup_ghidra.json`.

- `0040face` zeroes vehicle+4de0 and increments once for each exact case-sensitive
  discovered ULRORMount1..3, RearWeaponMount and FrontWeaponMount name. It is a
  geometry mount count, not a guessed enable flag. `004d7962` confirms strcmp,
  recursive search and first-match behavior. `race_weapon_mount_count` reproduces
  only the count; complete mount construction belongs to the weapon runtime.
- `00419597` sets +4aec for the front sensing sector; the existing `RaceAiState.front`
  supplies this value. The round-robin sensing schedule must remain unchanged.
- `00485372` returns race+ef0 when countdown+3a8 is exactly0; otherwise it returns
  trunc(race_time+3b0+2). Both `0041aa67` and `004242cb` require that integer to be
  strictly below race_time. The activation also requires vehicle+134 exactly0.
  `00484189` chooses the stored lock threshold0 or20 from its option bit.
- `0048355e` sets side delay to rand()%30+15, then later resets unrelated timers+14
  and +10 with two rand()%500/1000 draws, followed by rear+0c=rand()%1500/1000.
  Separate side/rear reset helpers accept the draw at its actual call site so the
  caller cannot accidentally collapse the shared original RNG sequence.
- `004831c3` reads keyboard `Controllers/Keyboard/Data.ini` [INPUTS] fields
  ACTIVATE_WEAPON, ACTIVATE_FRONT_WEAPON, ACTIVATE_SIDE_WEAPON,
  ACTIVATE_REAR_WEAPON, ACTIVATE_NITROUS_BOOST and DISPLAY. Loader uses existing
  archive/PSE precedence, validates DirectInput codes0..255 and has no fallback.
  Installed effective PSE values are29,2,3,4,5,15 respectively. The loose INI
  differs (current57), so the integration must load the effective configuration.
- `00424d3e` dispatches only on each binding's held-seconds==0 edge. DISPLAY/cycle
  precedes selected activation, then separate front, side, rear and boost actions.
  HUD item2 activates nitrous,3/4/5 activate front/side/rear. Individual slot actions
  test presence; selected actions do not add an invented presence check. Portable
  down/pressed inputs preserve that ordering; the host clock must retain short
  presses across frames without simulation ticks and consume pulses only once.
- Guided missile constructor `0044e820` assigns vtable005f5594; slot+1c contains
  raw thunk0040121c to0044fbf1, which calls0044f950. The selector finds its owner
  by matching weapon+26c chassis identity, clears target+08, then chooses along
  route order. It does not choose the nearest Euclidean car or filter destruction.
- Comparator004511a0 handles wrapping using abs(node difference)<=count/2. Same
  nodes compare each car's XZ distance to its actual004185fe(node,fraction0,branch)
  sample. Guided/Back flag+2c4 and owner wrong-way00415bc8 select forward/rearward
  direction. Wrong-way is dot(original forward, position-next-main-center)>0.
  Sequential candidate order and replacement preserve original ties.

Expanded standalone tests passed for mount exact names/duplicates, strict lock gates,
reset draws, player input holds/pulses/cycle order, PSE precedence, malformed binding
failure, wrapped and same-node missile selection, wrong-way/back variants and the
original absence of a destroyed-car target filter. No original-runtime equivalence
or complete weapon runtime restoration is claimed.

## Confirmed fresh Single Race weapon lock caller

The menu entry004aac8f initializes its48-byte race-options value at EBP-248,
writing byte+28=1 at004aacdd. The copy at004ab258..004ab268 passes all12dwords
to004a4f79, whose second copy at004a5240..004a524d forwards them after the
three leading race-constructor arguments. Thus options+28 reaches00484189
EBP+3c. Assembly004841ca..004841dd masks its low byte and sets race+ef0=20;
zero instead selects0. Track summary helper00492980 updates only options+2c
(the lap count);00494a1d reads+28 without changing it. Fresh Single Race
therefore uses the20-second lock. The explicit diagnostic override may select0
for tests, but0 is not the restored Single Race default. The original getter
00485372, strict activation gate and inclusive HUD/scene gates remain distinct.
Retained full decompilation and assembly: single_race_weapon_lock_ghidra.json.
This establishes the caller flag, not complete menu/garage reconstruction.
