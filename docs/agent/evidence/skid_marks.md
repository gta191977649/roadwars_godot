# Original tire skid strips

Program RoadWars; SHA-256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra evidence, 2026-10-22: skid_marks_ghidra.json.
Original source and class ownership remain unresolved.

`00413425` enables skids only for grounded tires with the original sliding-angle
and speed gate, or full braking while moving. Dust is a separate smoke input.
Position is wheel contact plus .1 Y. Material record offset20 supplies ARGB;
offset24 is the separate tire-particle color. `00412834` supplies half the smaller
raw tire X/Z dimension as width factor.

`004e8306` configures five strips per tire and sample interval
.125 / [ROADWARS] SKIDMARK_SCALAR. This scalar is distinct from SPECIAL_EFFECTS_SCALAR.
`004e849c` begins a new strip on entering a skid, advances the circular five-strip
index on leaving, and samples once when the accumulated interval expires. Excess
time is discarded, rather than producing multiple samples per callback.

`004e86f0` commits a sample; `004e8928` updates the live endpoint. Both rotate the
previous-minus-current displacement around Y by90*.0174555554986 radians and
multiply X/Z by the supplied half tire width. **The imported D3DRMVectorRotate
normalizes its result.** The earlier assertion that these marks have a
speed-dependent width was wrong: inspecting only the RoadWars caller missed the
import's behavior and produced visibly oversized, widening strips.

The separately hash-bound installed Microsoft D3DRM.dll (file version5.2.3790.0,
SHA-256 `9325747683e9961beaf4a203098df159dd4e9858e9cc24f417a2fc10451528cb`)
exports VectorRotate at preferred VA6d8e8ea8. Its instruction at6d8e8fa9 calls
VectorNormalize(result) before returning. The normalization is over full XYZ
displacement, before the caller ignores offset Y; sloping travel therefore
reduces horizontal width. A zero vector becomes unit+X in normalization helper
6d8e835a (assignments6d8e83c9..6d8e83d4). The portable implementation preserves
this fallback. These DLL addresses are not RoadWars addresses. Neither binary
was executed or modified. See skid_marks_reaudit_assembly.json for call sites,
DLL normalization and identities.

On level ground each pair is exactly twice the supplied half tire width, whether
sample displacement is .01 or10 units. U is0/1; V advances with sample index. The fifteen-sample index wraps in
the current strip, with the live endpoint resetting its committed count when
the sample index becomes1 again.

`004e8bda` draws the resulting triangle strip with xxSkidMark.bmp and its original
alpha companion: unlit, fog disabled, Z writes disabled, ordinary depth testing,
source-alpha/inverse-source-alpha blending. Godot owns native meshes, materials
and instances; the portable emitter owns all sampling and strip state. The host
converts the strip to alternating triangle indices and updates only dirty strips.

Fields written as5/20 at offset2bdc are not used as inferred lifetimes. A hash-bound
search for that displacement found only writes in004e849c, and the examined draw
routine contains no fading or expiry. Five rolling retained strips are implemented;
no invented timed fade is added. This negative search is scoped to the inspected
binary instructions, not a general claim about all effects.

Constructor/destructor ownership and the smoke subcall remain partial RE-STUB
mappings; numerical strip helpers have RE mappings. Fixture and integration tests
do not constitute original-executable differential verification.

The revised fixture rejects speed-dependent width at .01/.1/1/10-unit steps,
checks full-3D slope normalization and zero-vector fallback, and fills all32
vertices before verifying that the sixteenth sample wraps safely to four. The
retained original alpha material path was also re-audited: xxSkidMark.bmp uses
its raw-index alpha companion through ARGB4444; draw004e8bda uses an unlit FVF142
triangle strip, fog off, Z writes off and source-alpha/inverse-source-alpha.
The native adapter's 0x125 material flags express these states, and its alternating
triangle indices preserve strip connectivity. No material or winding change was
needed for this width root cause.
