# Original collision audio

Program: RoadWars. SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Evidence date: 2026-10-22. Database access was read-only; the original executable
was not run. Original C++ class/path ownership remains unresolved.
See `collision_audio_ghidra.json` for retained assembly and decompilations.

## Samples and voice commands

`0044d760` loads Sfx/Land.wav into0062a148 and Sfx/Crash.wav into0062a128,
using `004dd876` with argument0, which calls QMDX_OpenWaveEx flags1. Actual
installation samples are PCM mono16-bit22050Hz: Crash24255 frames (1.1s),
Land33189 frames (1.50517s). They are read through ResourceStore and the shared
bounded PCM decoder. No variants, random selection or alternate ground/material
impact samples appear in the recovered wall, wheel-landing or car/car audio paths.

Each accepted event calls `004dd8d4` to allocate a channel through QMDX_FindChannel.
It supplies an explicit3D source position, frequency reset(-1), zero source
velocity, priority0, flags0 and no callback: a one-shot at the sample's native
frequency. Volume is the global SFX scalar at globalsettings+0x924; `004ddb75`
multiplies by32767 and truncates before SetVolume. There is no impact-speed gain
or randomized pitch in these call sites. The portable command records the same
quantized gain, pitch1, non-looping playback and zero velocity.

The stored vehicle handle is overwritten by new events without stopping earlier
ones. Independent voices can overlap; a backend must allocate a fresh voice per
accepted event, not restart a single shared player. The exact legacy global
channel exhaustion/stealing policy belongs to QMDX and is not reconstructed by
this portable event layer. Native backend capacity is a host policy, not an
alleged recovered per-vehicle voice count. Reset/exit must stop native voices.

## Track walls and body/ground contacts

`0042e8f0` wall response retains cached pre-response speed in local70. After its
physical response it tests speed>3 and vehicle+0x42d8<=0. The accepted event starts
Crash at the solver's contact X/Z, with Y equal to vehicle worldY plus chassis
world-relativeY, sets0x42d8=.5 seconds and spawns the existing spark burst. Thus
wall sound and sparks share exactly the same gate. Assembly at0042e9e3 pushes
refresh=1 to00414cf7 before preserving local pre-response speed; the decompiler
omits this argument. Each actual hit refreshes cached speed before its physical
response, including repeated solver hits. `0047fff6` subtracts delta
from this timer and clamps it to0 before invoking wall processing. Multiple wall
solver iterations within the same frame cannot emit extra sounds after acceptance.
The restore-previous fallback does not invent a wall event.

`0042a11c` checks each suspension wheel as it regains contact. Once the desired
wheel height is inside its lower suspension limit, nonzero prior air_time triggers
landing response; audio additionally requires air_time>.5 seconds (strict).
It starts Land at the vehicle world position from `00428a64(0,1)` before clearing
the wheel's air_time. There is no shared cooldown, impact-velocity threshold or
normal-angle gate on this sound; four wheels may emit four one-shots in one frame.
The portable suspension helper calls CollisionAudio at this exact original branch.
Ordinary consecutive grounded frames cannot retrigger it because air_time resets.

The chassis front/rear ground-strike routine `00429bdf` applies body impulses and
position correction but has no audio call. It must not emit invented Crash sounds
for every chassis/ground intersection. Ground landing audio belongs to the wheel
airtime branch above.

## Other-vehicle collisions and coverage

The corrected stock update order follows00480b40: integration/ground orientation,
wall processing at00481046, final speed refresh at0048109d, automatic transmission
at004810b6, then suspension/chassis landing at004812f1. The previous portable
profile performed suspension and final speed refresh before walls. Moving these
calls restores the recovered order without changing solver constants; it also
puts wall audio before landing audio in a frame.0042c670(0), between automatic
transmission and landing, refreshes the5b70 wheel world-point cache without
re-querying the earlier ground/material records.0042a11c moves suspension meshes
but does not rewrite5b70. Material effect metadata therefore retains the earlier
material record; particle/skid geometry uses the post-wall/pre-landing5b70 points
plus the original+0.1Y. The portable profile captures those points explicitly. See
`collision_update_order_ghidra.json` for caller assembly.

`0041de2a` performs pair-impact response. Its audio gate requires param9>1000
and the receiving vehicle's separate+0x42dc timer<=0. Acceptance starts the same
Crash sample at its impact position/chassisY and sets the timer to.2 seconds.
`0047fff6` decrements this timer only while positive, without clamping the crossing.
`0041e180` derives the param9 scalar from the modulus of the difference between
mass-scaled contact-point velocity components; this is not a relative-speed
threshold in m/s. The overlap fallback in `0041e5bd` also computes a mass-scaled
vector modulus before calling this response. Passing ordinary vehicle speed as
param9 would be wrong.

CollisionAudio exposes vehicle_contact(original_impact_strength, original_position)
with the recovered strict threshold and independent timer. Tests cover it. The
single-vehicle driving profile does not yet have a reconstructed two-car collision
solver, so it never fabricates these events. Recovering this event contract does
not mark the complete pair physics function implemented.

Other references to the shared Crash sample include replay event6 (`0049dd5e`)
and weapon/object impact paths (`00450b3c`, `004a00d2`). These are distinct callers,
not extra WAV dependencies for stock collision feedback; their dispatchers remain
outside this single-vehicle profile. Replay must consume recorded effects rather
than run the car/car threshold a second time. Full race/replay/weapon ownership
and side effects retain RE-STUB coverage.

## Tests

`collision_audio_test` checks exact3/.5/1000/.2 boundaries, independent timers,
multiple same-frame landing voices, command sample frequency/gain/pitch/velocity,
reset and zero-delta event clearing. It drives the actual suspension landing
branch and proves three qualifying wheels emit once while the exact.5 wheel does
not. `vehicle_probe` loads both original WAVs and checks the driving profile's
Crash count exactly equals the wall effect requests. A particle request can be
rejected by its own occupied pool while its independent audio voice still starts.
These tests establish the
portable contracts; they do not claim native QMDX channel-policy equivalence or
original-executable acoustic differential validation.

The updated original-asset20-second probe passed with14 actual wall contacts,
5Crash commands (equal to5wall effect requests;4accepted spark bursts) and1Land
command. Both original PCM
resources decoded with the frame counts above. Collision and contact tests passed
after a targeted portable rebuild.
