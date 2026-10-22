# Vehicle combat integration coverage

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
VehicleCombat is a new portable coordinator, not a claimed original class name.
Original constructor/vtable evidence remains in weapon_runtime_ghidra.json; the
individual guided/contact/rear/special/projectile/particle ledgers retain raw bodies.

## Attachment and callback evidence

0040face searches FrontWeaponMount, RearWeaponMount and ULRORMount1..3.
Assembly0040fb64..0040fb70 always writes the same side pointer: the last existing
numbered side mount wins. 004cfc3d independently finds ROR1 and the selected mount
name plus Weapon. It does not select RORN. Real body/weapon/ammunition MSH files
provide the actual attachment/muzzle nodes and own-root bounds. 004d2ea1 resets
the attached weapon root transform, preserving its muzzle children.

004242cb uses cached speed (00414cf7(0)) plus3. Reservation004cfe98 requires a
nonpending mount, nonzero ammo and weapon virtual+2c readiness; ammo is consumed
before deployment. Front/rear callbacks fire next callback; rear adds180 without
normalization. Side004d03d6 rotates ROR1 roll (Euler+bc,004d5730), not pitch, at
180deg/sec to-90 and fires on the following callback. The distinct vertical helper
004d0132 is not called by side dispatcher004d0725.

Retraction tests virtual+24: Rocket/Guided/Excalibur thunk004017df ->00406f50
returns constant1, whereas Gatling thunk004022f2 ->0044d710 tests active+270==0.
Missile mounts consequently retract during flight. Base virtual+2c for rear,
Spikes and Ram is00406f70 (inactive). Rocket virtual+3c thunk00401690 ->0049f7eb
is a no-op, so non-guided jam commands must succeed without changing state.

The local advance method retains a bounded owner-only reference harness. Live
integration uses stable callback tokens, inline registration/removal observers,
and dispatch_callback for one actual object at a time. The native004d72a9 list is
GLOBAL and its search operation mutates the dispatch cursor. Registering a new
callback searches to the old tail; the subsequent Next can immediately execute
the new tail within the captured count. Removing a head sets cursor to the new
head, after which Next skips that new head. A blanket next-pass rule is false.
combat_callback_cursor_ghidra.json retains exact helpers/assembly. Complete global
interleaving includes vehicles, turret controllers, independent impact pools and
unrelated world effects; owner-only tests do not establish full RNG equivalence.

Guided00450b3c computes a height bound but never tests it: assembly00450cf2..d1b
and00450ed7 confirm XZ intersection directly enters force/damage. The coordinator
deliberately omits the common bullet vertical gate for guided weapons. Owner grace
is age<=.5 for all four types, even Back Missile's .1-second launch phase.

0044f365 refreshes target selection through0044f950 only on accepted launch and
copies the result to its active pointer+2d0, distinct from the AI query pointer+8.
VehicleCombat requires select_guided_target immediately before guided.launch;
RaceSession owns the identity latch and supplies that identity's current position
on later guidance frames. A query refresh must not retarget a missile in flight.
The all21 actual-asset harness checks the selector is called exactly once for
each guided launch and never for rejected duplicate requests or other branches.

Particle/effect counts, impact registration, damage and shared RNG are explicit
services. Missing required services return unavailable; no empty-pool fallback.
Impact events include both world and vehicle contacts plus rear explosions.
Rear impulses use scalar1, not simulation dt. Oil004f8594 consumes one rand()%4
per emitted visual triangle (maximum49 per patch despite collision capacity50), retained as VehicleCombatOilVisual.atlas_frame.

## All 21 factory branches: evidence and test coverage

All original C++ class names remain unresolved. The following constructor/vtable
identities establish object ownership without assigning guessed original names.
VehicleCombat owns typed portable instances; it is a new coordinator.

Test abbreviations: W=weapon_runtime_test (factory/parameters/mount reservation),
P=weapon_projectiles_test, M=weapon_muzzle_visuals_test,
G=guided_weapon_runtime_test, C=contact_weapon_runtime_test,
S=special_weapon_effects_test, R=rear_weapon_effects_test,
I=rear_weapon_instances_test, T=weapon_trails_test,
K=skeletal_mesh_runtime_test, V=vehicle_combat_test,
L=race_session_combat_lifecycle_test (optional original-asset profile),
A=mesh_attachment_test.

V's optional original-asset profile loads every branch on every installed car.
It also runs each branch on one installed car with isolated typed services and
actual WeaponTrailPool instances for 1,800 fixed steps. It checks reservation,
duplicate rejection, ammo consumption, attached missile visibility, timed callback
completion/readiness, and successful reload clearing owned callbacks, visual
instances and queued audio. Spikes are correctly passive: original default ammo0
rejects activation while its force effect still applies. Mines/Arachnoids survive
without targets; their destruction is tested by reload, not a fabricated timeout.
This profile is an owner-only harness, not the global RaceSession callback stream.

L additionally searches diagnostic seeds1..12 and covered all21 selected branches
through the live RaceSession service/global callback path. It checks the original
20-second lock without ammo consumption, zero-step pulse retention, pause during
deployment and in-flight effects, catch-up debt, active-effect restart and scope
exit, restored initial ammunition and cleared activity/deployment. The integration
owner executed it successfully. These lifecycle checks apply to every table row;
they do not assert every branch-specific victim-hit/damage condition.

| Factory name | Constructor / vtable | Normal use checked | Restriction checked | End and destruction checked | Still missing a dedicated integrated assertion |
| --- | --- | --- | --- | --- | --- |
| Machine Gun |0045ae10 /005f56d0|P alternating real muzzles, capacity15 and speed; M fade/roll; V activation|P duplicate begin, cadence; W/V ammo and pending gate|P burst end/rearm and projectile expiry; V30s callback drain/reload|Vehicle hit plus impact/audio/pool cleanup in one Machine Gun session scenario|
| 20mm Cannon |0040c300 /005f5178|P speed90/capacity5; M single flash; V two actual synthetic contacts|V duplicate reservation, ammo exhaustion; P strict1s boundary|P timed end; V ready, drained callbacks, reload|Independent Cannon impact-pool/audio source ordering versus a native trace|
| Electron Cannon |00433710 /005f53b4|S first participant/edge and four-side dt damage; K actual SKA; V activation|S owner excluded, XZ height ignored, duplicate readiness gate|S strict1.5s, wait actual impact count, remove beam/callback; V timed drain/reload|Moving victim velocity, SKA pose, audio and impact-pool lifetime asserted together in live session|
| Plasma Blaster |0047d000 /005f582c|P speed110/capacity5; M flash; T actual trail; V activation|P1s threshold; W/V pending/ammo gates|P common trail-count stop; V timed drain/reload with real trail pool|Plasma-specific live contact, sound and independent impact cleanup assertion|
| Battering Spikes |00407ec0 /005f50b8|C normalized force+1000 and full model-length extension; V passive actual asset|C front angle/vertical angle/zero force; V ammo0 rejects trigger|Passive has no timed expiration; V successful reload releases object|Actual vehicle-pair collision/damage/armor result with Spikes equipped|
| Hydraulic Ram |00455bc0 /005f560c|C additional1000 only extending, 2units/sec mesh travel; V activation|C exact half-length threshold, repeated begin reset-before-gate, rear contact rejection|C retract to0, cancel animation semantics; V timed drain/reload|Actual pair collision damage at extending versus retracting frames|
| Vulcanized Gun |004ce560 /005f59e0|P speed70/capacity5/impulse; M flash; T burst and expanding sprite; V activation|W/V ammo/pending; P strict1s|P timed end; V timed drain/reload|Vulcan contact spawning both independent impact callbacks and complete session cleanup|
| Flame Thrower |004352f0 /005f5460|S100 particles, creation/animation draws and four side contacts; V activation|S owner skip, strict Manhattan1.5, pool capacity, missing RNG|S strict3s plus delayed particle drain/readiness; V timed drain/reload|Native rendered flame appearance/audio and integrated damage over same lifecycle|
| Rocket Launcher |0049f330 /005f590c|P acceleration, hit and horizontal impulse; T smoke; V attached body and activation|P owner/height contact rules; W/V pending/ammo|P strict>5 expiry, contact/drain callback, flare30deg/sec reuse; V drain/reload|Actual mounted-to-detached transform and independent impact/audio validated in one session|
| Gatling Gun |0044c720 /005f54d8|P five muzzle cycle/capacity30; M flash scale/roll; V activation|W side deployment, duplicate/ammo; P cadence|V30s callback drain/reload; mount waits weapon inactive|Complete Gatling burst/hit/pool/audio sequence through RaceSession|
| Guided Missile |0044e820 /005f5594|G heading10, homing sign draw, jam and ground clamp; V mounted body/activation|G active launch rejection/jam; V pending/ammo; static collision has owner grace.5 and no height gate|G impact/trail cleanup and inactive motion/RNG; V expiry/drain/reload|Guided-specific live target acquisition and owner-grace contact assertions|
| Back Missile |0044e820 /005f5594|G heading170 and .1 launch phase; V attached backwards body/activation|W rear heading+180; V pending/ammo; owner grace remains.5|V expiry/drain/reload; shared G cleanup|Back-specific jam/contact/owner-grace assertions|
| Swarmers |0044e820 /005f5594|G four mesh-size offsets and eight tracking draws; V four attached bodies/activation|G per-missile jam/cadence; V pending/ammo|G expiry; V per-missile trail drain/reload|Mixed impacted/live swarm, independent burst callbacks and global RNG asserted together|
| The Excalibur |00434b90 /005f5408|G main-route sample and close-target shortcut; V activation with owner route0|G missing owner node/route rejection; V pending/ammo|G strict>20 expiry; V timed drain/reload|Real predecessor rank changes, branch crossing and target loss in one live session|
| Smoke Screen |004a6110 /005f5960|R emit state; T actual smoke geometry; V activation|R strict3 boundary; V missing service fails explicitly|R>3 deactivate/remove; V callback drain and reload|Independent remaining smoke particles after weapon callback removal and session teardown|
| Oil Slick |00474270 /005f57ac|R projected triangles and wheel contact/grip; V50 collision/49 visual triangles and49 atlas draws|R strict>.2/travel>4, capacity50, four-use limit, route wrap pruning|R end>.9 and remove; V timed callback drain/reload|Real wheel friction, visible patch and audio asserted together|
| Missile Jammer |00464f50 /005f573c|R three draws, color, heading and duplicate whole-weapon jam; V activation|R strict XZ radius30, owner skip; V pending/ammo|R strict>5 plus next cleanup/removal/audio-stop flags; V drain/reload|Session loop-handle stop and all simultaneously jammed missile owners asserted together|
| Grenades |0044db70 /005f5534|R cyclic five-instance deployment; I falling/fuse/internal damage/impulse; V activation|I active launch rejection, strict radius6 includes owner; W/V ammo|I .75 fuse/removal; V timed callback drain/reload|Real blast callback particles, Boom source and damage ordering in integrated session|
| Land Mines |004563d0 /005f5660|R five finite slots; I footprint/grounded wheel hit; V activation|I owner grace based previous age<2, airborne exclusion; R pool exhaustion|I hit removal; no invented100s expiry; V reload destroys surviving mine|Integrated grounded contact/armor bypass and silent blast assertions|
| Reactive Mines |0049ce70 /005f5894|I distinct extra-effect flag, upward internal impulse; V activation|I owner grace and shared grounded test; R finite pool|I hit removal; V reload destroys surviving mine|Reactive secondary particle callbacks and damage/cleanup in same live scenario|
| Arachnoids |00406320 /005f5024|I ownerheading-180, walk1.5, leg headings, wall RNG; V attached leg assets and deployment|I unavailable wall RNG, actual ground/wall service; R finite pool|I explicit stop; V reload destroys surviving walking instance|Arachnoid-specific victim contact/owner grace and simultaneous leg/blast teardown|

## Current implementation and verification limits

The native muzzle subtrees, guided/Rocket flares, Arachnoid leg subtrees, Electron
SKA animation and Jammer color/heading are exposed to presentation. Audio events
come from accepted activation, actual contacts and actual rear deployments;
Jammer loops belong to RaceSession. These are implemented interfaces, not omitted
visual/audio algorithms. Native host appearance and sound still require the
parent's rendering/audio validation, rather than marking them complete here.

Mounted missiles now follow004cfe98: request-time world Euler sums generate
004cf787 formation offsets, which004d5750 stores as local positions after reset.
This unusual double transformation is preserved. Back Missile local heading is180.
Child flare visibility starts hidden, becomes enabled at first launch and persists
on reuse. Accepted guided fire audio uses missile0 actual position: assembly
0044f7c3 loads weapon+2d4 with no index, adds0xc and calls004d3ee3. This matters
for Swarmers whose first missile has a nonzero formation offset. The exact
listing is retained in guided_fire_audio_source_ghidra.json. Guided0044fc04 performs speed/tracking/motion/trail work even on the final
inactive callback; cleanup then hides, marks ready and resets age. The previous
portable early return skipped that work and has been removed, with a regression
checking its additional tracking draw and moved hidden position.

The previous MSH attachment concern is resolved by direct binary evidence, not
by decomposing its serialized basis into guessed Euler angles. Those vectors are
metadata; native runtime Euler starts at zero. All three vectors are now retained,
and actual body springs/ROR1 dynamic Euler use the recovered summed-angle ancestry
for mount, muzzle, flare, HoodPoint and RearWeaponMount poses. Side housing has
separate fixed/ROR1 mesh instances. See mesh_attachment.md and its raw evidence.

Remaining verification limits are not resolved by the table:

- No per-branch original-executable replay trace exists. Fixed-step kernel and
  portable session determinism do not prove native global RNG equivalence.
- Live L covers all21 branches for pause, zero-step input, catch-up, active restart
  and scope exit. The branch-specific victim-hit/damage assertions above remain
  required; generic lifecycle success does not prove each original destructor or
  exact native damage/effect ordering.

## Executed checks

Warning-enabled GNU C++20 direct builds of VehicleCombat and guided runtime passed.
vehicle_combat_test passed synthetic Cannon contacts, reservation/exhaustion,
transactional failed loads, unavailable smoke services, and49 oil atlas draws.
Its optional original-asset profile passed every branch on every installed car
and the 21-row isolated lifecycle profile described above. The guided test passed
its new inactive-cleanup movement/RNG regression. A passed synthetic matrix and
metadata checks plus all12 installed cars. V passed again with chassis pitch15/
roll20, body pitch11/roll17, native housing and mounted missile visibility. The
integration owner also ran L successfully for all21 branches (seeds1..12). Previous direct checks passed
muzzle visual families, skeletal matrices/animation, scene Electron traps, weather,
projectiles and guided flare persistence. No core build was launched concurrently
with the integration agent. Parent-owned core/Godot/global checks are recorded
separately and are not claimed by this ledger.
