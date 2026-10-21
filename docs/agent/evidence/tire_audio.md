# Original tire sliding audio

Binary: RoadWars Kopio, SHA256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra decompilation and call-site assembly retained in
`tire_audio_ghidra.json`, 2026-10-22. No original executable or library was executed.
Original class and C++ source ownership remain unresolved.

## Confirmed resource selection

`00436270` reads the friction-material array (552 bytes each). The null-terminated
260-byte filename at material offset `0x120` is the sliding sample; the separate
filename at `0x1c` is rolling sound. The prefix at `00613ef0` is `sfx\`.
Literal `none` yields no sample handle. Each material has three associated words;
`004366f5` selects a friction triangle and copies both its material and sound words
to the vehicle's per-wheel slots. The first wheel material begins at vehicle
`5074`, its longitudinal grip at `5078`, and its skid sample handle at `5914`.

The Dark Forest archive friction materials contain `skid.wav` in all five material
records, including a dirt material with longitudinal grip equal to float 0.6.
The sample itself is read through ResourceStore's loose-file fallback:
`Sfx/skid.wav`, SHA256
`1bef8b072ab5db55a3728546edeef377a01fdcc890a2d18fbcf1f0200c830f2e`,
21570 file bytes, mono unsigned 8-bit PCM, 22050 Hz, 21376 frames.
No source assets or absolute machine paths are copied into the project.

## Confirmed trigger and voice lifetime

`00413425` evaluates grounded, moving wheel slip or full-brake predicates and ORs
those predicates across all four wheels. This is the same predicate that requests
skid strips, distinct from dust-only tire particles. If any wheel skids it calls
`0041330f`; if none skid it stops channel `4798`, resets that handle to -1, and
clears the active-skid latch `5974`.

`0041330f` sets latch `5974` on entering a skid BEFORE checking longitudinal grip.
It starts a voice only when the old latch was clear, channel was absent, and
FIRST wheel grip is strictly greater than float 0.6 (`005f513c`, bytes 9a99193f).
Thus a continuous skid that begins on low grip remains silent after entering a
higher-grip material until all wheels stop skidding and a new skid begins.
The first wheel supplies the sample even when only another wheel is skidding.
Changing materials during an existing audible skid does not swap its sample.
These unusual rules are preserved rather than replaced with a generic brake sound.

The original `004dd8d4` call uses repeat count -1, no completion callback,
original SFX gain and priority zero. `004ddbb1` supplies zero source velocity;
`004ddbda` supplies frequency -1 (sample-native rate). Active frames update
source position through `004ddc03`. There is no tire speed/RPM pitch modulation,
volume ramp, or stop-on-key-release rule beyond the recovered wheel predicates.
The portable commands use full-sample looping, pitch scale 1, fixed SFX gain and
zero source velocity. Godot owns playback and PCM signedness conversion.

## Implementation and validation

`tire_audio.*` loads all distinct material-selected skid samples and emits voice
commands without any Godot dependency. `TrackCollision` retains each selected
material's filename; `VehicleDynamics::tire_audio_input()` exposes the actual
per-frame OR predicate, first-wheel raw material grip and selected sample path.
The host must use `TireAudio::active_sample_path()` for the playing voice, since
current contact metadata may change while a voice continues.

`tests/core/tire_audio_test.cpp` checks looping, native pitch, fixed volume,
start/update/stop, first-material retention, low-grip rising-edge suppression,
no delayed retry, stop/reentry, no-sample behavior and invalid input. The optional
configured installation argument checks actual Dark Forest material selection
and original skid PCM dimensions. Direct C++20 build and this original-resource
check passed. Root native integration and driving audio smoke tests are separate.
QMDX source attenuation, channel competition and allocation failure are not
recreated by the nonspatial own-car native playback profile. Rolling audio is a
separate original channel and remains outside this sliding-sound implementation.

## Deterministic original-asset driving fixture

After the vehicle's recovered speed/slip cache order was restored, the previous
smoke sequence (accelerate/steer/S) no longer crossed the audible rising-edge gate.
The S key is a recovered direction-sensitive brake/reverse ramp, so that sequence
was not a valid guarantee of full braking. No vehicle parameters were changed.
A read-only original-asset portable simulation probe tested Space onset at frames
30, 60, 90, 120, 180 and 240; all crossed the audible gate. The retained automated
fixture accelerates straight for 60 frames at 1/60 second, applies recovered Space
full braking for 60 frames, then coasts for 60. At frame 60 the actual first-wheel
grip is float 0.9, cached speed exceeds 9 m/s (observed 9.50035), and one original
Sfx/skid.wav loop starts. On skid exit it stops. The optional installation path
case in `tire_audio_test.cpp` checks the real vehicle predicates and exact sample;
that test passed against the configured installation. No audio state is injected.
`drive_test_step` now accepts an optional sixth `handbrake` input, default false,
to drive the same Space input from the native SFX smoke test. The test asserts
actual driving activation before beginning isolated PCM mixer fixtures.
