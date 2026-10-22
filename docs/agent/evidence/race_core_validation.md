# Portable competition acceptance

Date: 2026-10-22. Reference SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
This record describes reconstructed execution using original installed resources.
The original executable was not run. It does not establish original-runtime
equivalence or native-renderer pixel correctness.

## Executed commands and results

| Command | Observed result |
| --- | --- |
| `roadwars_race_route_test` with the configured installation | All24 original Lap/LapRev resources across12 tracks decoded; actual Dark Forest crossing predicates completed a118-node lap. |
| `roadwars_race_session_test` without an installation argument | Invalid race options and empty-session lifecycle checks passed; full original-asset acceptance remains opt-in. |
| `python scripts/smoke_competition.py --core --laps 3 --frame-independent` | Default six-car Dark Forest/Animal race finished after18603 ticks,302.05 race seconds. Identical complete per-tick state at10/15/30/60/75/120/144/240 render FPS and350/900ms jitter/stalls. |
| `python scripts/smoke_competition.py --core --ai 0` | Dark Forest/Animal, one lap, zero opponents:6854 ticks,106.233 race seconds, player rank1. |
| `python scripts/smoke_competition.py --core --track "RWA Speedway" --car Sarge --ai 5` | Second track/vehicle, six cars, one lap:3268 ticks,46.4667 race seconds, player rank6. |

The final matrix uses the staged core: all vehicle pre-collision work, original
wall/pair dispatch and bounded neighbor rechecks, then all vehicle post-collision
work. Older results predating this stage-order correction are superseded.
Logs are generated under ignored `build/competition_*.log`; no original assets,
absolute installation paths or reconstructed binaries are committed.

## What the acceptance checks establish

The diagnostic player uses the same recovered AI controller as its opponents.
The test never writes checkpoints, increments laps, teleports a vehicle to force
progress, or injects a result. Normal controller-triggered recovery remains active.
The three-lap player finished fourth. Daisy, Heartbreaker and Electra had finished;
Kitiara and Romulus remained on lap2. These unfinished participants remain
unfinished in the result, rather than receiving invented finish times.

The reference trace compares float bit patterns after every simulation tick:
all vehicle poses, velocities, angular rates, RPM, controls, transmission, wheel
airtime/heights, wall counters, complete AI controller/output fields, route and
guard state, lap times, finish times, rankings, particle sources and collision
audio events. Every frame-rate schedule uses the same seed and input history.
The fixed60Hz clock is the authorized modern host policy, not an original
frequency claim.

Debug snapshots are read every15 simulation ticks in comparison runs. They must
match actual participant/tick/time values, and leave serialized simulation state
unchanged immediately and throughout the subsequent complete trace. The reference
run does not read debug snapshots. This checks read-only diagnostic behavior;
native checkbox interaction and projected geometry need separate host checks.

Each race also checks unique characters, countdown throttle lock, three initial
restarts, a five-second paused update during live racing, stable finished results,
post-finish restart and repeated seeded initialization. HUD inputs must contain
the actual player rank, completed laps, race length and finished marker.

## Limits

The route predicate fixture supplies crossing positions directly and is a focused
geometry test, not a driving acceptance substitute. The session acceptance is a
real simulation from the starting grid through the recovered controller/physics.
It covers the default forward track and a second forward track; it does not prove
all track/vehicle combinations or arbitrary collision pileups complete.

Weapons, damage, elimination, networking and persistence are intentionally outside
this no-weapons profile. Route editor branches, unconfirmed original class names,
exact legacy SIMD approximations and original-executable equivalence remain open.
See `race_route.md`, the AI and pair-collision evidence, and the parent competition
record for detailed reconstruction and host-rendering limits.
