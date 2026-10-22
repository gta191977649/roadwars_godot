# Live Single/Quick Race menu and menu music

## Silent Loading gain-command correction (2026-10-22)

The earlier scheduling fix did not resolve the underlying silent-start defect.
RaceMusic::gain set volume fields but left action as none; EngineAudioAdapter
explicitly ignores none commands. A theme started at zero gain therefore remained
silent despite subsequent fade updates. gain now emits EngineVoiceAction::update,
using the existing shared voice contract for Loading and Garage. This is a
reconstruction command-contract bug, not evidence of original-game behavior.

The previous playing/nonzero-gain smoke could miss this because its large menu
steps started the voice after the intro had already advanced. The new native
regression starts Loading at exactly zero, checks gain at 150 and 300 ms, and
isolates the theme on its own audio bus for one second. Native Vulkan measured
peak -0.785719 dB and passed Quick-first, Single, Garage/store and menu return.
This verifies actual mixer output, not physical speaker listening. Core CTest
67/67 and native adapter CTest 2/2 passed; core and extension builds, progress
update/check, boundaries and whitespace checks passed. No extra production delay
was introduced for this correction.

## Unified race Loading audio scheduling fix (2026-10-22)

Both Single and Quick use apply_frontend_race_request and update_frontend_audio.
Previously resource preparation could begin while the character voice still had
zero or very low intro gain. The first synchronous asset stage could then block
before the next gain update. This was especially exposed by Quick's direct
track-to-Loading path. The shared loader now updates audio before asset work and
waits for the portable 0.3-second Loading intro to complete. Live Loading receives
monotonic elapsed time, matching the frontend clock, instead of Godot's render
delta. Completion fade and race music handoff retain their existing path.

The 0.3-second initial presentation interval is supported by 004aa5f3 in
menu_custom_screens_ghidra.json, bound to the reference hash below. The scheduling
fix is host integration, not an additional recovered-function completion claim.
Race-menu smoke now tests Quick first with no prior Garage/theme warmup and
requires intro completion before intermediate race asset progress.

Validation: core and extension builds passed, core CTest 67/67 and native adapter
CTest 2/2 passed. Native Vulkan Quick-first/Single/Garage race-menu smoke and full
headless frontend lifecycle smoke passed. Progress update/check, boundaries and
whitespace checks passed. Playback assertions do not claim physical listening.

## Garage music reinspection (2026-10-22)

The SHA256-matched input was rechecked and functions 00438393, 0046305d,
00443569 and 00462ab9 were read again through Ghidra. Retained follow-up:
garage_music_followup_ghidra.json. Garage starts the selected character theme
before its Loading milestones with volume argument 100. The no-CD fallback
loops ThemeMusic.wav at MUSIC gain. Store transitions retain that voice;
the store playback call uses the door SFX and SFX gain, not a new theme.
Garage exit calls 00462ab9, which stops the voice, releases the sample and
resets both handles. These are static observations, not original runtime audio.

The existing portable music_path() already selects the character theme for
modify_vehicle, including all store variants, at full gain. Native Vulkan
race-menu smoke passed stronger assertions for Garage playing/nonzero gain,
playback advancement over 150 ms and continued audible playback into the weapon
store. No extra production playback branch is necessary. Speaker listening
and CD transport remain outside this validation.

Loading music follow-up (2026-10-22): the host now loads the character selected
by MenuFrontend directly, matching music_path(), rather than reading a separate
race-request snapshot with an Animal fallback. This also keeps preview and Garage
audio selection consistent when no live race request has been prepared. The native
race-menu smoke additionally requires decoded frames and nonzero gain before
Loading finishes; a playing but silent voice no longer satisfies that check.
This is an integration correction using the existing 0046305d evidence below,
not additional original-executable behavioral verification.

Follow-up validation: core and Godot extension builds passed. Native Vulkan
Single/Quick race-menu smoke passed the added nonzero-gain Loading assertion,
Garage/store continuity and return to main. Full headless frontend previews and
lifecycle smoke passed, as did native adapter CTest (2/2). Physical speaker
listening was not performed.
The initial core CTest run passed 66/66. After the final rebuild, race_session
reported its contract success line but then segfaulted, including on a targeted
rerun; it remains an unresolved regression. The other 65 tests passed after
regenerating stale progress mappings. Progress and boundary checks passed.

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra results: single_quick_menu_flow_ghidra.json and
menu_race_launch_ghidra.json. All binary observations are static, not recordings
of the original executable. Original C++ class/source ownership remains unknown.

## Confirmed flow and reconstruction

004ab60d distinguishes SINGLE RACE (race+24=1) and QUICK RACE (race+24=0),
then calls 004aac8f. Both select character and track. Only Single calls 004927c0
for StartRaceSummary; Quick bypasses it. The live portable menu retains its mode
through character, track, Summary/Garage and back navigation. Returning to main
clears that mode so Season/Multiplayer previews cannot inherit a race request.

Summary displays the actual deterministic participant selection, zero initial
points, the selected track's DATA/NUMLAPS, OUTPUT/TRACKLENGTH(M) converted to miles,
and AI/TIRES. It no longer substitutes unrelated preview racers or 3 laps/2.50
miles. Start Race emits a portable request rather than an unavailable action.
Summary pointer hitboxes follow the actual Garage/Start icon locations.

004aa4d3 initializes a fresh garage player using 0040d4cb and STREET/POOR tires.
0040d4cb clears weapon/drivetrain strings, initializes cash to 25000 and calls
armor installation with NONE/NONE (target strings at 00612a9c/00612aa4). A fresh
Single player therefore keeps stock equipment, no mounted weapons and original
body texture (portable paint index 0), while opponents use recovered random
loadouts. RaceOptions can carry that player setup without consuming random
loadout draws for it. CLI diagnostic --race and Quick retain all-racer random
loadouts. Stock body/damage textures now use the original Body directory.

004aac8f prepares Characters/<selected>/16Portrait.bmp before 004aa5f3 Loading,
then calls 004a4f79. Live Loading uses this portrait and the original loading
layout. The host now advances a main-thread coroutine across rendered frames,
yielding completed track/player/audio/vehicle/effect preparation milestones and
per-AI progress. It does not reset GameEntry inside an active callback. Percentages
represent completed work units, not byte throughput; individual units can still
block a frame. Loading time never enters the fixed simulation accumulator.

GameHost consumes the request outside simulation callbacks, releases frontend
owners, then uses the existing RaceSession/fixed-60Hz race path. Pause Continue,
Restart and Exit to Main preserve existing behavior; return clears the live race,
restores frontend scheduling and starts menu music. Results continue to main.
No render delta changes or alternative physics path were introduced.

## Original menu BGM

00462bed attempts CD playback conditionally, then falls back to the exact
Sfx/SoundBackgroundMusic.wav resource with game+920 volume (ROADWARS/MUSIC).
The portable music owner decodes PCM or Microsoft ADPCM and issues a native
looping voice. It starts at Title/main-menu time, remains one voice through
menu pages, switches to character music during Loading/Garage, releases before
race startup and restarts on return/results. Publisher AVI audio stays separate. The CD device branch,
legacy QMDX ABI and exact fade envelopes are not implemented.

## Scope and limits

Single/Quick launch/navigation and return are live. Garage/store browsing and
back navigation are supported; purchase/trade-in/repair economics remain explicit
unavailable business actions, and product views remain labeled preview data.
No purchase is silently applied. Season persistence, multiplayer and option
persistence are outside this user-confirmed scope. Summary record persistence,
legacy Quick rand%2 detail, player cache restoration across separate races,
replay and original-runtime pixel/audio parity remain incomplete. Containing
original functions stay analyzing; these fixes do not advance them to verified.

## Validation

See the executed acceptance appended after final checks. Native menu test captures
are ignored build/frontend_captures artifacts, not redistributed game assets.

## Unified unavailable-frontend assertion (user-requested extension)

All unimplemented business activation paths now show a shared portable modal:
ASSERTION FAILED / Not implemented, plus the rejected action. This is a modern,
recoverable frontend assertion, independent of NDEBUG and the debug overlay.
Generated reconstruction stubs still use the existing fatal not_implemented helper.
The modal blocks page navigation/scene advancement and dismisses through Enter,
Esc or its OK hitbox. Dismissal clears only the rejected action. Pending Replay
assertions do not resume a paused race. Modal input takes priority over debug UI.

Season/Multiplayer activation from the main menu reports the missing business
feature instead of pretending to start a partial flow. Unsupported option writes,
bindings and Garage purchases use the same modal. Explicit read-only previews
remain available to diagnostics. Disabled original selections retain their original
unavailable feedback; missing assets remain actual errors rather than being
misreported as unimplemented features.

## Executed acceptance (2026-10-22)

Portable core and Godot extension builds passed. Core CTest 66/66 and native
frontend_video_audio/debug_overlay CTest 2/2 passed. Installed-asset MenuFrontend
and RaceMusic acceptance passed, including stock player request, native lap count,
Quick Summary bypass, pointer assertion dismissal and menu MUSIC gain.

`python scripts/smoke_frontend.py --races` passed under native Vulkan and headless:
main-menu BGM was playing with nonzero sample count/gain and advancing playback;
Single visited Summary/Garage/back/Loading and started the selected Dark Forest
2-lap six-car grid with stock player and random AI; Quick skipped Summary and
randomized every car. Both returned through the original pause menu to main with
BGM restored. The same run checked Season's recoverable assertion, modal back
handling, and Replay's shared assertion without advancing/unpausing the race.
Native Loading, Summary, Garage and assertion screenshots were visually inspected.
The Loading portrait is visible before initialization; original pixel equivalence
and physical audio-loop listening are not claimed by the automated checks.

Existing complete headless competition smoke passed results-to-main navigation,
restart and teardown. Existing headless frontend smoke passed every screen and
variant preview plus host restarts. The eleven-rate frame-independence probe plus
jitter and 350/900ms stalls retained bit-identical 1200-tick physics traces. No
simulation frequency or formula changed. Progress update/check, portable boundary
checks and diff whitespace checks passed. An early CTest run found the newly added
BGM source mapping stale; regenerating progress fixed it, and full reruns passed.


## Character music, staged loading and wheel correction (2026-10-22)

Additional hash-bound static evidence: garage_character_music_ghidra.json.
0046305d reads character CD configuration and falls back to looping
Characters/<character>/ThemeMusic.wav at ROADWARS/MUSIC gain. 00438393 starts
that voice before Garage loading. Its milestones are STARTING (0), GARAGE (10),
STORE (20), TIRE (30), ARMOR (40), WEAPON (50), DRIVE TRAIN (60), REPAIR (70),
PAINT (80), FINISHED (100). The portable loader prepares and caches those scene
and row owners in matching stages. 00443569 switches stores without restarting
music. Garage exit stops the theme; the wrapper resumes menu BGM. Race Loading
uses the character voice with the evidenced 0.3-second fade; Garage/store keeps
full gain. CD transport remains unavailable. These containing functions remain
partial; coroutine scheduling and resource caching are modern infrastructure.

Five installed mono MS ADPCM themes have a short final block whose fact count
exceeds encoded samples by exactly one. Decoding now bounds that specific case
to available samples; larger discrepancies remain errors. Synthetic tests cover
both bounds, and all twelve installed character themes decode/loop/gain tests
pass. No original-runtime PCM parity or physical listening is claimed.

Avenging Angel's exported Chassis root is (-0.94053626, 0, -2.40654492).
Live body presentation replaces that root with VehicleState, while tire physics
and presentation previously added it again to child anchors. The corrected
loaders normalize the live chassis root before deriving tire/ground and combat
attachment offsets, for player and AI. Installed-asset wheel tests compare all
four axle and ground anchors with raw local children for all twelve characters.
This is a coordinate correction, not new reverse-engineering completion credit.

Staged loading reuses the renderer; return-to-menu now detaches and releases the
old sky before destroying its texture. Consecutive Single/Quick launches no
longer retain a stale native sky. Native Vulkan menu smoke passed monotonic
multi-frame progress, visible intermediate bars, theme continuity through weapon
store, both race launches and main-menu return. Loading and weapon store captures
were inspected. Resource units are still synchronous and need not have equal
cost; this is not a guarantee of hitch-free loading on every machine.


Final regression for these changes: core incremental build passed; core CTest
66/66 passed after regenerating the new character-theme source mapping; native
adapter CTest 2/2 passed. The existing Godot extension build and native Vulkan
Single/Quick smoke passed, followed by final headless race-menu, full frontend
preview/lifecycle and competition smoke passes. Portable eleven-rate 1200-tick
and native timing 300-tick comparisons passed, including irregular schedules,
350/900 ms stalls, engine pause and teardown. Installed all-character wheel and
music probes passed. A targeted native Avenging Angel Single/Quick probe captured
the player after 600 simulation steps; the Single image was visually inspected
and shows attached rear wheels. AI uses the same corrected geometry/presentation
loaders, but a separate Avenging Angel AI screenshot was not inspected. Actual
speaker listening and original-executable runtime comparison remain unperformed.
Progress update/check, boundary and whitespace checks passed.
