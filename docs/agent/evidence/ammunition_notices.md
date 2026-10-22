# Transient ammunition and use-count notifications

Reference RoadWars x86 PE SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static read-only Ghidra decompilation is retained in ammunition_notices_ghidra.json.
This is a recovered subset; original source/class ownership and runtime pixel
parity remain unverified. No original executable was run.

## Observed producers and layout

004242cb emits a message only after004cfe98 accepts weapon activation, and only
for the selected player. It uses the full weapon display name at weapon+214,
uppercases it, appends the exact one-space string0061373c and the post-request
mount ammunition at vehicle+4e20+slot*40. 004246c0 similarly requires successful
nitrous activation00473e8e and appends space00613760 plus remaining uses to the
localized NITROUS BOOST source key. Rejected, locked and empty attempts emit no
new message. The existing mount/equipment owners already debit on acceptance;
the notification observes that result and never decrements gameplay counters.

There are four text/timer entries at HUD+1ccc/1dd0, stride108. Each producer scans
in order and takes the first zero timer OR matching name prefix, sets timer2.0,
and writes the latest full label/count. When all entries are occupied and none
matches, a new label is omitted. An earlier empty entry wins over a later matching
entry; this original detail is retained rather than globally deduplicating.

0048cd1b draws centered at x320, with first top y440; subsequent fixed lanes move
up by Font1 height+5, even when a lane is empty. 00487fbe initializes that font
with Fonts/16Font1.bmp, top ARGBb2ffffff and bottom66ffffff. At remaining>=0.5,
those colors apply. During the final0.5 seconds, both colors become white with
alpha trunc(remaining/0.5*255). This preserves the native color transition rather
than replacing it with a generic multiplied fade. Zero timers are invisible.
0048cf43 suppresses these messages after finish/elimination and in the native
cockpit branch. English source-key labels match the current frontend language;
arbitrary language lookup remains outside this focused change.

## Reconstruction and authorized presentation changes

RaceSession owns four message lanes. Successful player weapon/nitrous requests
update them; the final fixed step decrements live timers. Pause and zero-tick
presentation do not age them. Restart clears them. Drawing reads only snapshots,
so render FPS cannot determine lifetime or alter ammunition. Messages remain
independent of the selected left-side map/equipment panel. CPU/native diagnostics
expose each text/timer and the actual weapon/nitrous counts for acceptance tests.
The new test-step hook injects ordinary InputState requests through GameEntry;
it does not inject text, spoof acceptance or mutate ammunition directly.

The user requested separation from DEV BUILD. The diagnostic watermark retains
its requested bottom-center location, with glyph scale0.4 and a three-pixel bottom
margin. Its independent small row sits below the native y440 caption plus full
glyph height and at least five pixels of clearance for the shipped font. The
four original notification lanes remain unchanged. Both groups share the same coordinate policy in each view,
including isotropic cockpit rendering. Bottom notifications are also shown in
the modern cockpit as an explicit user-facing extension; original cockpit
instrument/readout reconstruction is unchanged. The original player-only
notification ownership is retained during the modern Inspect camera feature.

## Validation

Core queue tests cover acceptance labels,
case/prefix refresh, four-slot capacity, earlier-hole ordering, invalid deltas,
expiry and reset. Installed-asset HUD checks assert original row geometry/colors,
uniform final fade, presentation purity and disjoint watermark/message bounds.

Executed acceptance (2026-10-22): portable core and Godot extension builds passed;
core CTest passed 67/67 and native adapter CTest passed 2/2. The installed-asset
race_frontend_test passed, including final watermark clearance assertions.
Native and headless smoke_ammunition passed with actual successful weapon and
nitrous requests, rejected locked requests, all four camera views, presentation
purity, pause/resume, expiry and restart. Native captures were inspected at
1280x720 (external and cockpit) and 1920x540 (external): the four captions and
small bottom-center watermark have separate bounds. These captures establish
reconstructed-renderer presentation only, not original-runtime pixel parity.
Frame-independent and timing race regressions, headless competition, cockpit,
and frontend race/lifecycle smokes also passed. The final watermark-only layout
change was subsequently checked by installed-asset assertions and both ammunition
smokes; it does not change simulation scheduling.
