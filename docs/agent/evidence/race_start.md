# Single-race countdown, opening camera and speech

Target SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static read-only MCP decompilation is retained in race_start_ghidra.json; relevant
assembly and constants were also inspected. Original executable was not run.
RaceStart is an inferred portable owner under unresolved, not an original class.

## Confirmed original behavior

00484189 reads SECONDSTILLGO (installed9). 0048355e resets countdown and calls
004848ee. 004848ee resolves StartCam1/2/3 via004bfb94 (absent nodes return zero),
positions the initial eye at camera1, and targets the average projected grid
positions. 00484bcb clamps eye above road+1 before movement; remaining in[5,7)
selects camera2 and[3,5) selects camera3. 00480b40 invokes camera mode9 with twice
the callback delta while remaining>=3. 004e553d applies independent axis velocity:
if abs(error)/2<=abs(velocity), velocity=error/2; otherwise velocity+=error*dt;
velocity*=scalar, eye+=velocity*dt. Constructor004e2cc7 initializes scalar1,
velocity zero. Aim points at average grid position. The forward start path is
reconstructed; reverse camera lookup and other global camera modes are not.

Below3,00480b40 dispatches00408e09 into the stock chase/acquisition path004094fb.
The pre-acquisition eye advances toward the chase target at distance/(remaining-1),
using divisor0.1 only when remaining-1<=0. After countdown reaches0, the rate is
max(speed+10,30). Arrival switches to acquired chase. Existing portable chase
coverage limitations remain; this does not promote all004094fb branches.

00487fbe loads Sfx/3.wav,2.wav,1.wav,Go.wav. 0048b120 truncates the positive
remaining float directly (assembly0048b146), triggering each cue once at digits
3/2/1/0. It displays nothing for digits>=4 or remaining0. Digit0's actual string
at0061a6e4 is `G`, not the previous invented `GO!`. At logical(320,240), scale is
2*(1-fraction), white alpha is trunc(fraction*255)/255; assembly0048b2e2..0048b372
confirms the fade direction. HUD dispatch0048cf43 suppresses normal HUD until
remaining<3. The countdown uses its separate Fonts/16321go.bmp atlas loaded by00487fbe,
with alphabet321G at0061a13c. G selects the complete GO graphic, not a literal
letter. Ordinary HUD text continues to use Fonts/16Font1.bmp.

## Integration

RaceSession advances RaceStart once per fixed tick after vehicle updates, before
countdown decrement. Pause freezes camera/timer and does not repeat speech.
Godot only submits camera/HudQuad/PCM commands; no Tween or independent timer.
Restart clears acquired state, velocities and all four speech flags. Native audio
voices also pause/resume with the session. Failed samples/configuration report
errors; PCM and Microsoft ADPCM reuse existing decoders. Resources are not copied
into the repository. No simulation steering, RNG, collision or lap logic changed.

## Limits

These checks compare reconstruction/static evidence, not captured original runtime.
General camera target modes, reverse races, CD playback, localized font variants
and full shared camera ownership are not newly claimed complete. New mappings
remain analyzing, including partial functions marked RE-STUB.

Native competition music now waits until the original countdown<1 transition,
matching00480b40; trace mode preserves its immediate free-driving music entry.

## Executed validation (2026-10-22)

Core and Godot builds passed. Core38/38 and Godot40/40 CTest passed after updating
source mappings (the initial registry checks correctly rejected stale mappings).
Original-asset HUD test checks dedicated atlas loading, digit width, centering,
scale/alpha, missing resources and release of the extra texture. Native clean
intro captures show the original orange/yellow3/2/1 and GO! artwork, including
all camera stages; previous ordinary-font screenshots are superseded. Native
no-debug intro verifies all four cues and clean audio/render teardown. Native
debug competition completed a real race, results/navigation and three restarts.
Portable complete-race matrix includes camera basis/eye and pending sound order
at10/15/30/60/75/120/144/240FPS and350/900ms stalls; per-tick results match. It
also tests pause/restart and one-shot cue order. RWA Speedway/Sarge passed.
No original-runtime audiovisual capture comparison was performed.
