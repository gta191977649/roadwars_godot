# Original combat audio contracts

Executable SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static evidence: `combat_audio_ghidra.json`. QMDX.dll SHA256:
`1dc420da3c9d704a454da1e47b5750bf60597f7d05fe5ab9c550cd75b7dbae29`;
retained PE-export/Capstone evidence: `combat_audio_qmdx.json`. Neither original
executable nor DLL was run. Original class/source names remain unresolved.
Portable owners and events remain in unresolved and do not claim original names.

## Samples and original call semantics

Shared loader0044d760 opens18 combat dependencies plus the separately implemented
Crash/Land samples. Exact path strings were read from memory, rather than inferred
from Ghidra labels: the actual `wa-chain.wav` and `wa-heat.wav` paths contain
hyphens that Ghidra symbol labels replace with underscores. All18 installed PCM
samples decode successfully through ResourceStore and the existing bounded decoder.

| Trigger | Sample | Evidence |
| --- | --- | --- |
| Accepted Machine Gun/Gatling activation | Sfx/wa-chain.wav |0045b387,0044cccc|
| Accepted20mm Cannon activation | Sfx/cannon.wav |0040c7e5|
| Accepted Plasma activation | Sfx/Plasma.wav |0047d4d8|
| Accepted Vulcan activation | Sfx/vulc.wav |004cea45|
| Accepted Rocket/four guided activations | Sfx/wa-heat.wav |0049f83a,0044f365|
| Accepted Electron activation | Sfx/electrongun.wav |00433da1|
| Accepted Flame activation | Sfx/FlameThrower.wav |00435682|
| Bullet vehicle hit: Machine/Cannon/Gatling | Sfx/MGImpact1.wav |0047dd34,0047e5e0|
| Plasma/Vulcan vehicle hit | Sfx/PlasmaHit.wav;Sfx/VulcHit.wav |0047dd34,0047e5e0|
| Same bullet wall/ground hit | same hit sample at half SFX gain |0047e5e0|
| Rocket/guided contact | Sfx/Boom.wav at full gain, actual hit point plus .1 Y |0049faaf,0044fc04|
| Grenade explosion | Sfx/Boom.wav |0044e1c4|
| Mine/Reactive/Arachnoid actual deployment | Sfx/MineLay.wav |00456a40|
| Electron contact when existing sound timer permits | Sfx/electronBuzz.wav |00434128|
| Oil accepted activation | Sfx/Oil.wav with repeat argument4 |00474472|
| Jammer activation/retry | Sfx/jammer.wav with repeat argument-1 |00465217,00465395|
| Race-owned nearest damaged vehicle loop | Sfx/fire.wav |00484d8f,00484f6d|
| Race-owned nearest active Nitrous loop | Sfx/nitrousboost.wav |00484e27,00484f6d|
| Selected player internal HP reaches0 | Sfx/eliminated2.wav, no explicit source |00414d2a|

Firing audio occurs once per accepted original weapon activation, not per individual
burst bullet. The turret uses the same actual weapon objects, so it shares these
sound contracts. The recovered contact-weapon and Smoke activation routines have
no sound call. Grenade activation itself has no sound call. Mine/Arachnoid contact
00456bc7 spawns its explosion pools and damage but contains no Boom audio call;
the portable interface does not invent one from the explosion's appearance.

Ordinary004dd8d4 playback resets frequency to native(-1), source velocity to0,
priority to0 and uses gain trunc(globalSFX*callScalar*32767)/32767. There is no
random pitch or speed-based gain in these weapon call sites. Each accepted ordinary
event allocates a fresh voice and can overlap earlier events. Stored handles do not
imply stopping an earlier one-shot at weapon deactivation. The nonspatial elimination
branch uses QMDX polar position(0,.5,0), direction(0,0,1) and360-degree cones.
The portable event records its lack of a world source explicitly.

### Repeat argument confirmed inside QMDX

QMDX_PlayEx export18003390 forwards its fifth argument through18005190 to the
command constructor1800a920. That constructor stores the requested repeat count
at command+20 and initializes completed repetitions+1c to0. Completion1800b0a0
restarts while those fields differ, increments+1c unless requested==-1 and uses
the final segment when the requested count is reached. Thus0 is one initial play,
Oil4 means four additional repetitions/five total plays, and-1 repeats indefinitely.
This is verified DLL control flow, not an assumption from the caller's literal.
The portable `original_play_count` field retains that exact repeat argument.

## Loop ownership and source updates

Jammer activation starts its per-object channel with completion notification.
00465395 retries when its handle is-1, updates its actual owner position and
velocity each callback and stops/releases it on the inactive cleanup branch.
Backend allocation failure and completion determine handle liveness; the portable
adapter must report that liveness rather than fabricating a successful channel.

Fire and Nitrous each use one race-owned channel.00484f6d adds actual delta to its
timer and only acts when timer>.05, resetting to0 with no remainder/catch-up loop.
It scans original participants for eligible sources and chooses the nearest by
full3D Euclidean distance; equal distances retain the first participant. No distance
cutoff is added. Existing channels move to the newly selected source without
restarting. Missing channels restart with native frequency, indefinite repeat and
completion notification. If nobody qualifies, stop and clear the shared handle.
Every accepted update supplies the selected source position and vehicle velocity.

00484e27 marks Nitrous eligible for any nonzero nitrous timer, including its trail
drain phase.00480b40 computes vehicle2eec=1-internalHP/maxHP;00484d8f marks Fire
eligible only when this is strictly>.5. Position is the original00428a8f body world
point, not an invented camera point. Engine audio remains owned by its existing module.

Mine deployment's source pointer+d0 is correct: constructor004566f4 adds4 before
mesh wrapper004d7676, which adds another+c before SpatialObject. Mine positionX
therefore starts+d0. Grenade0044de81 has no extra4, so its positionX starts+cc.
The retained constructor listings prevent mistaking this layout difference for an
audio coordinate bug.

## Portable interface, validation and limits

`load_combat_audio` loads the18 original samples and SFX scalar atomically.
`combat_audio_event` maps evidenced accepted events; silent branches return ok with
no event, unknown identities return invalid_argument and unsupported cue combinations
return unavailable. `combat_audio_command` preserves numerical gain, native pitch,
raw repeat count, priority, spatial intent and completion notification. Source identity
and voice ownership belong to the caller's vehicle/turret/race object. The loop helpers
emit start/update/stop commands and accept backend liveness, without owning a backend.

`combat_audio_test` validates sample/cue selection, half-gain impact quantization,
silent branches, Oil/Jammer repeat metadata, elimination spatial intent, nearest
source/tie ordering, strict loop timer boundary, updates, stop and invalid delta.
Warning-enabled standalone build and synthetic tests pass. The optional installed
resource profile loads/decodes all18 actual samples and also passes.

These are portable resource/control contracts. QMDX channel exhaustion, attenuation,
Doppler/cone DSP and final audio mixing are not reconstructed here. A Godot backend
must not replace missing native spatial algorithms with unlabelled default attenuation.
Emission wiring, backend playback and audible original-runtime comparison are separate
validation obligations; successful PCM decoding is not proof of equivalent spatial audio.
