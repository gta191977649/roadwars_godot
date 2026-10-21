# Original engine sample and voice control

Program RoadWars; executable SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Date 2026-10-22. No original executable or DLL was executed or modified.
Retained evidence: engine_audio_ghidra.json and engine_audio_assembly.json.
The latter also identifies the separately inspected QMDX.dll by its own SHA-256;
its addresses must never be interpreted as RoadWars addresses.

## Sample and playback lifetime

`0040d4cb` constructs `Cars/<vehicle>/Engines/Sound.wav` and loads it through
`004dd876`, which forwards to QMDX_OpenWaveEx. No second engine voice or a per-car
engine2/mix/min-max-pitch configuration was found in this initialization/update
path. The implementation must not invent a synthesized replacement or extra layer.

The actual Animal file is RIFF/WAVE PCM format 1, mono, 16-bit, 22,050 Hz, with
136,346 data bytes / 68,173 frames. It contains fmt and data chunks, with no smpl
or other loop-range chunk. `0041373a` starts the wave once a channel is absent,
at the current source position, repeat count -1 and priority 2. `004dd8d4`
initializes native sample frequency, zero source velocity and SFX volume, then
calls QMDX_PlayEx. The whole original clip is looped. `004137a7` stops/relinquishes
the channel when its audible parameter is false.

`00484cae` passes audible true only when the camera/source XZ **Manhattan**
distance is less than 75. `0047c7df` assembly proves the calculation is
abs(dx)+abs(dz), not Euclidean distance. Y does not affect this gate.

## Frequency and volume

For the human-driver path, `004137a7` starts from the cached RPM getter:

```text
n = truncate_toward_zero(rpm + 500)
n = min(n, 10000)
loaded frequency = truncate(4*n + n*n/3000)
coasting frequency = truncate(4*n - (integer_divide(n,2) + 800) + n*n/3000)
```

The decompiler omits the +500 and much of the integer/floating calculation;
assembly at 004137f1/00413811 and 00413841..004138e6 is authoritative. The 3000
constant is float32 at 005f5218. The resulting integer is passed directly to
QMDX_SetFrequency by `004ddbda`. Native playback therefore uses pitch ratio
frequency_hz / original_sample_rate, with no arbitrary min/max pitch ramp.
Examples: idle coasting 1,033 Hz; RPM 3,000 coasting 15,533 Hz; loaded 18,083 Hz;
the capped loaded command is 73,333 Hz. The unusual low idle command is preserved.

The voice update timer is strictly greater than float32 0.025, then resets to zero
without retaining its excess. Starting the voice seeds the timer with 0.025 so
the first positive frame applies pitch immediately. Volume changes only when
the loaded flag differs from its last value: loaded gain = SFX, coasting gain
= 0.75*SFX. `004ddb75` truncates gain*32767 to an integer. The portable command
retains that integer and the normalized gain rather than substituting dB tuning.

The original loaded flag at vehicle+4ed8 is reset to zero by race initialization;
the previous flag at +4edc is initialized to one. A bounded whole-text literal
offset scan found reads and zero writes but no positive write to +4ed8. This does
not exclude every possible indirect mutation. The current profile uses the
observed false state; it does not guess that this flag means throttle > 0. The
loaded-curve branch is preserved and tested for future state recovery.

Original engine RPM calculation itself needs the 00413688 wheel-circumference
division and *60, followed by rear-end and gear ratio; the earlier pseudocode
omitted those return calculations. The drivetrain reconstruction now owns that
cached RPM and supplies it to audio. No independent guessed RPM is computed here.

At sound-update ticks, source position and velocity commands are also produced.
Velocity is normalized to magnitude 111.75 when the original cached speed exceeds
111.75; current velocity magnitude is used for normalization. The default human
chase-camera path uses the actual vehicle position. A separate first-person
condition can offset the source forward by twice vehicle length and remains open.

## Portable/native boundary and limits

`engine_audio.*` contains bounded RIFF PCM decoding, resource loading, frequency,
cadence, volume, audibility and voice command state with no Godot dependency.
The decoder supports PCM8/PCM16 mono/stereo and rejects malformed/compressed data.
Audio sample bytes remain outside version control; game_dir is resolved only
through the existing ResourceReader and ignored local configuration.

`EngineAudioAdapter` uses native Godot AudioStreamWAV and AudioStreamPlayer,
one original loop, source sample rate, recovered pitch ratio and recovered gain.
It performs the required unsigned-WAV8 to signed-Godot8 conversion. Its lifecycle
stops/removes the voice on scene restart, exit, failed initialization and diagnostic
fog fixtures. This single-car adapter is deliberately nonspatial: QMDX 3D distance
attenuation, cones, Doppler, hardware channel arbitration and camera-mode source
offsets are not claimed matched. Original source/velocity commands are retained
for that future boundary; arbitrary Godot 3D defaults are not substituted.

The full RoadWars audio routines remain partial RE-STUB mappings. Tests of the
reconstruction are not original-runtime differential equivalence.

## Validation

Direct GNU C++20 build/run of engine_audio_test.cpp passed, including malformed
RIFF, compressed format rejection, alignment errors, frequency curves/clamp,
25-ms boundary, integer volume, audible threshold, start/stop/restart and source
velocity limit. The optional configured-install argument also passed against
the original Animal file and confirmed 68,173 frames, 22,050 Hz and SFX 0.9.

audio_smoke_test.gd checks the native voice advances, engine frequency responds to
driving, the Master audio mixer reports measurable sample output, and the voice
is released by diagnostic-mode transition. Its GPU/audio execution is performed
by the parent integration pass; do not call it passed without its success marker.
