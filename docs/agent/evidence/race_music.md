# Race background music

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Ghidra MCP program `RoadWars`, x86 Windows, image base `00400000`.
Retained queries: race_music_ghidra.json and race_music_volume_ghidra.json.
Static evidence is not runtime observation.

## Confirmed original behavior

- `0048355e` opens `Sfx/RaceMusic.wav` with `004dd876(...,1)` when the CD
  availability predicate `004f77b3` fails and the race is not the excluded mode.
  The returned wave handle is race field 0x45c; the voice is field 0x458.
- The race update `00480b40` calls `00462cbc` once when countdown is below one
  and normal racing starts. `00462cbc` reads the track CDMUSIC timing fields and
  tries the CD transport when PLAYMUSICFROMCD is enabled. Failed/unselected CD
  playback falls back to Sfx/RaceMusic.wav at unchanged sample rate, infinite
  repeat count -1 and global INGAMEMUSIC (field 0x954). The result/menu music path
  uses the separate MUSIC volume (field 0x920) and is not this race BGM.
- `004dd8d4` submits the nonspatial QMDX play request; `004ddb75` multiplies
  volume by the 32767 scale and truncates before setting the channel volume.
- `00462ab9` stops CD transport, stops a valid wave voice, closes its wave and
  resets the handles. Its unrelated CD/wave backend mechanics are not recreated.

## Portable reconstruction and limits

RaceMusic is a new portable owner under unresolved; no original class or source
path is asserted. It loads the original resource, reads INGAMEMUSIC, emits one
loop-start command and an explicit stop command. Native playback uses the existing
Godot PCM adapter with a separate OriginalRaceMusic voice, preserving engine/SFX.
The driving profile already starts in the racing state, so BGM begins at race
initialization. Static preview does not start music. Vehicle reset does not restart
music; scene restart and all exits/diagnostic fixture transitions release its voice.
The host has no CD transport and therefore selects the original WAV fallback.
Original countdown, CD audio and result music remain incomplete. Mapped functions
stay analyzing because this is a subset of their original behavior.

The shipped fallback WAV is mono Microsoft ADPCM, 22050 Hz, 512-byte blocks,
1012 samples per full block, with a final partial block and a fact count of
1857152 frames (about 84.22 seconds). Its source SHA256 is
`2fa20d604a292471ba07de6a991a6a9a85ee86850435b1cee7ca63c62c66830c`.
decode_ms_adpcm_wave is new platform-independent codec infrastructure replacing
the QMDX codec dependency, not recovered original game code. It parses bounded
RIFF chunks, coefficient banks, mono/stereo blocks, signed nibble prediction,
saturation, adaptive steps and the fact sample limit. It does not carry RE markers.

The codec structure was checked against [Microsoft's ADPCM overview](https://learn.microsoft.com/en-us/windows/win32/xaudio2/adpcm-overview)
and [FFmpeg's codec implementation](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/adpcm.c).
Independent Windows ACM decoding resolved negative prediction rounding: arithmetic
right shift matches the original installation's entire fallback sample exactly.
This establishes codec output equivalence to ACM, not QMDX mixer equivalence.

## Validation

- race_music portable test covers mono/stereo header/sample ordering, signed
  residuals, negative prediction rounding, invalid predictor/truncated input,
  unloaded start, loop/gain command, repeated start and stop idempotence.
- Original-asset invocation decodes all 1857152 frames, reads INGAMEMUSIC=0.6,
  checks QMDX-scale volume 19660 and emits ignored build/race_music.pcm.
- `python scripts/validate_race_music.py` compares all 3714304 PCM bytes with
  the independent Windows ACM Microsoft ADPCM codec. Exact match, PCM SHA256
  `bc734e77ccb1e434ed7b53d07fbdf188add77784f55480bdbbbe67d4cac37c60`.
  The script follows [ACM stream conversion](https://learn.microsoft.com/en-us/windows/win32/api/msacm/nf-msacm-acmstreamconvert)
  and the packed [ACMSTREAMHEADER](https://learn.microsoft.com/en-us/windows/win32/api/msacm/ns-msacm-acmstreamheader)
  contract, including processing the final partial block. The adapter never calls
  ACM; it is solely an independent validation oracle. PCM/report remain in build/.
- `python scripts/smoke_race.py --music` isolates BGM before engine updates,
  measures native mixer signal, verifies gain and real sample duration, seeks near
  the actual end to observe loop wrap, checks vehicle reset continuity, three scene
  restarts and diagnostic/tree-exit voice release. Engine audio acceptance mutes
  BGM explicitly so music cannot mask a missing engine mixer signal.
  Final native BGM peak was -4.232833 dB. Engine, SFX and frontend smoke checks
  also passed after integration; the portable suite passed all 29 tests, and
  progress update/check plus the portable dependency boundary check passed.
