# Track rotating objects

Program RoadWars, SHA-256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra evidence collected 2026-10-22 is retained in
track_animation_ghidra.json. Original source ownership remains unresolved.

Confirmed: track constructor `004b7439` finds RotObject nodes and constructs
controllers with `004c7880`. Starting at name character 10, three decimal fields
(three digits, optionally preceded by minus) supply speeds for object fields
+0xb4, +0xb8 and +0xbc. Suffixes are not speed fields. The portable parser
rejects malformed names rather than reproducing unsafe legacy string reads.
Constructor mapping remains partial: controller pointers and cached world position
are not represented by the parser.

Confirmed: `004ba95f` updates these controllers using elapsed seconds.
`004c7b1b` adds speed * elapsed time to each nonzero-speed angle, with no wrap.
Getters `004d5790`, `004d57e0`, `004d5830` identify heading, pitch and roll and
sum ancestor angles. `004d3dbd` forms Rx(-pitch) * Rz(roll) * Ry(heading).
`004d3ee3` rotates local translation by the parent rotation before accumulating
world origin. Portable presentation retains object pivots and updates existing
renderer instances; Godot does not independently animate these objects.

Original installed MSH observations (assets are not versioned):

| Track | RotObject names | Count |
| --- | --- | ---: |
| Dark Forest | RotObject_004_000_000 | 1 |
| Underground | RotObject_010_000_000A/B; RotObject_-010_000_000C/D | 4 |
| Badlands | RotObject_000_100_000_ALPHA | 1 |
| Coliseum | RotObject_000_020_000 | 1 |
| Jackknife | RotObject_040_000_000 (suffix variants) | 4 |
| Sidewinder | RotObject_000_000_025_NOALPHA (suffix variants) | 2 |

All observed rotating nodes have no children. Dark Forest's node uses bridge and
guardrail textures; identifying it as a train is unsupported. Underground's nodes
use xTrain.bmp. This is texture evidence, not an original-runtime observation.
Animated collision, turret behavior, sound and original renderer caching/LOD
equivalence are outside this recovery. Static preview remains static.

Validation: track_animation_test exercises signed fields, suffixes, malformed
input, zero-speed branch behavior, unwrapped angles and pivot transforms.
track_animation_probe loads original Dark Forest and Underground resources and
checks that renderer bases change while pivots stay fixed, then releases scenes.
These checks do not establish original-executable equivalence.

Executed validation on 2026-10-22: portable core and Godot extension builds passed;
all 24 core CTest cases passed. Original-asset probe observed 2 changing Dark Forest
surfaces and 4 changing Underground surfaces with unchanged pivots and clean
scene release. Progress update/check and portable boundary checks passed.
Godot lifecycle smoke passed with 10 restarts, original-asset preview with 3
restarts and all RIDs released, and rendered driving smoke with acceleration,
steering, brake and reset passed. These general smoke captures do not isolate
the distant rotating models; their basis/pivot change is checked by the probe.
