# Publisher intro video adaptation

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.

The retained decompilation of `004ab60d` in `menu_frontend_ghidra.json` calls
`004ab401` for `Screens/Publisher Intro/Avi1.avi`, filling the display, followed
by `Avi2.avi`, centered and limited to 320x240. It then creates the Intro logo
screen and Title screen. `004ab401` delegates opening/updating/closing AVI to
`00455fc3`, `00455ec0` and `0045623d`; keyboard or mouse edges skip playback.
These are static code observations, not original-executable runtime observations.

## Confirmed installed data

Both RIFF AVI files contain one 320x240, 24-bit Cinepak (`cvid`) video stream
with scale 1 and rate 15, plus one mono Microsoft GSM 6.10 audio stream
(WAVE format tag 0x31, 22050 Hz, 65-byte blocks). Container chunk lengths, stream
headers and every video packet were read without running the original executable.

| Resource | SHA256 | Video frames | Video duration |
| --- | --- | ---: | ---: |
| Avi1.avi | `3239f3dc3685a0fb61824c34355a17f2dcf872381c7e83bfc022216dc0fa79ad` | 127 | 127/15 seconds |
| Avi2.avi | `021fff3950e09c92202cda5d4fb4338afa4a8e95bbb093c8f0cdfb854739ebea` | 176 | 176/15 seconds |

The files contain arbitrary old data inside RIFF JUNK chunks, including an
apparent `00dc` header. A global signature search would incorrectly select this
data. The decoder walks RIFF/LIST boundaries and reads packets only from `movi`
or nested `rec ` lists. Cinepak vector chunks can have one unused alignment byte,
whose value is not necessarily zero.

## Portable contract and limitations

`FrontendVideo` is new host-independent format adaptation, not a claim of original
class/source ownership. It owns packet data, parses dimensions and time base, and
decodes color/grayscale Cinepak codebooks, full/selective codebook updates, mixed
V1/V4 vectors and inter-frame skips. The decoded image is opaque, top-down RGBA;
the caller owns texture upload and presentation. `frame_at` advances all dependent
packets when time jumps forward and rebuilds decoder state when seeking backward.
Invalid timestamps and malformed nested lengths fail explicitly; a failed decode
does not overwrite the caller's output. No Godot or operating-system type enters
the core.

The format interpretation was cross-checked with the primary
[FFmpeg Cinepak decoder](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/cinepak.c).
No external decoder library or extracted asset is included in the repository.
Compressed GSM audio and its format metadata are exposed through `audio()`.
`platform/godot/extension/frontend_video_audio` converts those bytes through the
Windows Audio Compression Manager into `WavePcm`, with no Godot or Windows types
crossing into the portable controller. The Windows adapter requests synchronous
GSM 6.10 to mono PCM16 conversion at the original sample rate and verifies complete
source consumption/output length. It releases prepared headers and codec handles
before returning. Other platforms explicitly report unavailable until they provide
a codec adapter. No RE marker or function-completion promotion is assigned to this
new codec infrastructure.

This host choice follows the original delegation of media decoding to system
codecs rather than claiming reconstruction of the Microsoft GSM DSP. API behavior
is supported by Microsoft's primary documentation for
[acmStreamOpen](https://learn.microsoft.com/en-us/windows/win32/api/msacm/nf-msacm-acmstreamopen)
and [acmStreamConvert](https://learn.microsoft.com/en-us/windows/win32/api/msacm/nf-msacm-acmstreamconvert).
There is no downloaded library or new runtime dependency beyond Windows ACM.

## Validation

`frontend_video_test.cpp` exercises a synthetic RIFF/Cinepak movie with fixed
known V1 colors, all file truncations, malformed inner packet length, timestamp
validation, exact frame boundaries, end-time clamping, repeated reads and reverse
seek. The original-asset invocation decodes all 127 and 176 frames, checks opaque
RGBA, and verifies that a reverse seek followed by forward decoding reproduces
the final frame byte-for-byte.

`tests/platform/frontend_video_audio_test.cpp` additionally checks malformed audio
format/block lengths and transactional output, then converts the actual files and
repeats each conversion with byte-identical output. Avi1 produced 186560 PCM frames
with an absolute peak of 30184; Avi2 produced 258560 PCM frames with an absolute peak
of 32768. Both retain 22050 Hz mono 16-bit output. The audio durations are slightly
shorter than their video durations because the stored GSM stream contains whole
320-sample blocks. The caller uses the portable video time for start/seek/stop and
must not stretch the sample to match rounded container durations.

Standalone GNU C++20 compilation with `-Wall -Wextra -Wpedantic` and the test
invocation against the configured installation both passed on 2026-10-22.
The Windows ACM audio test also passed after standalone GNU C++20 compilation
with `-Wall -Wextra -Wpedantic` and `-lmsacm32`.
Windows AVIFile could open the streams but `AVIStreamGetFrameOpen` returned null
because the required system decoder was unavailable. Therefore no comparison to
a system Cinepak decoder or original-executable pixels is claimed.
