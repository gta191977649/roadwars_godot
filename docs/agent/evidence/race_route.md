# Race route, checkpoint and lap reconstruction

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
The configured installation's Kopio executable was read and its SHA256 rechecked.
No original executable was run. Read-only Ghidra decompilation is retained in
`race_route_ghidra.json`. Original source paths and original class names remain
unresolved; `RaceRoute` and its value types are portable representations of
observed track/vehicle fields, not recovered original class names.

## Confirmed observations

- Track constructor `004b7439` calls `004c5cc7`; the route loader chooses
  `Tracks/<track>/World/Lap.bin` or `LapRev.bin` according to reverse mode.
- The resource starts with 32-bit version and main-node count. Version4 stores
  a 100-byte main node, a separate branch-presence word, and an optional100-byte
  branch. Older loader branches discard32 bytes of name before each node.
  The serialized pointer at node+60 is discarded and never becomes a host pointer.
- Main fields: +00 one-based ID; +04/+10 crossing endpoints; +1c center;
  +28 half-width; +2c lateral vector; +38 optimal offset; +3c alternate offset;
  +40/+44 lateral bounds; +48 speed hint; +4c/+50/+54 integer AI flags;
  +58 accumulated distance; +5c branch kind; +60 runtime branch pointer.
  Alternate-node IDs need not equal main IDs: original Badlands reverse data
  demonstrate this. Some branch kinds contain0xcdcdcdcd; retain their raw value.
- `004182ee` selects a route point using the vehicle lookahead base+4ae8,
  current branch+4428, recent branch+4424, behavior+4bac and lateral offset+4ac4.
  Behaviors2/3 prefer kind3; other behaviors prefer kind2. A shared branch latch
  keeps successive lookahead samples on a selected branch. `0041817d` scans six
  upcoming points to select the optimal versus alternate lateral offset.
- If bound+40 is below bound+44, `004182ee` reverses lateral X/Z and all four
  offsets/bounds. The reconstruction normalizes value copies during decoding,
  preventing read-only debug access from mutating route state. Lateral Y is
  preserved, matching the original instructions.
- `004185fe` samples optimal-offset points and interpolates X/Z toward the next
  point, preserving the first point's Y. Ground projection is a distinct stage.
- `004be8f9` locates initial progress in the final30 route quads, starting at
  max(count-30,1). Missing placement is an explicit failure, not a nearest-node
  heuristic. `0048355e` calls it after placing each car at its Start dummy.
- `004b7439` builds five ordered lap guards as `(node_count-5)*i/5`, i0..4.
  `004bd136` extends the current motion endpoint by one normalized movement unit;
  tests the next crossing/alternate; otherwise tests the previous crossing.
  Forward crossings advance branch state and ordered guards. Crossing node0
  increases lap count only after guards1..4 are present. Reverse crossings clear
  later guards and clear an incomplete wrap. Recording-only AI-editor branches
  are outside the live race reconstruction.
- `0048355e` resets ten floats beginning at vehicle+4440. The portable race
  wrapper therefore accepts1..10 laps; this is the recovered storage bound.
- `004bf081` projects onto current/next or previous/current route segments using
  crossing directions and the original XZ intersection. It divides accumulated
  main distance plus projection distance by the loaded route length. Original
  constants are100 at005f5240, -10 at005f534c,10 at005f5094 and .01 at005f5318.
  `00413b93` suppresses rank progress before the first lap guard, and at node0
  before guard4. No global nearest-route search substitutes for this ordering.
- `004c5cc7` derives length from final main accumulated distance plus3D distance
  back to first center. The original writes this value to configuration through
  `00461cf2`; the reconstruction computes it in memory and does not persist it.

## Executed validation

Standalone GNU C++20 build with `-Wall -Wextra -Wpedantic` passed. The route test
rejects every truncated prefix, trailing bytes, unsupported versions and invalid
lap bounds. Synthetic fixtures cover version3/4, alternate nodes, pointer
discarding, lookahead bounds, rescue sampling, ordered guards, two complete laps,
repeat crossing, reverse guard clearing, initial-placement failure and rank gates.

All24 installed Lap/LapRev resources across12 tracks decoded successfully.
Dark Forest has118 nodes and computed length4087.97; its original crossing data
also passed a complete118-node lap/progression fixture. This fixture directly
supplies crossing positions to validate predicates; it is not an AI driving or
full-race acceptance test. Integrated driving validation belongs to the parent
race acceptance record.

The integrated portable race matrix is recorded in
[race_core_validation.md](race_core_validation.md).

## Limits

The initial-quad search and route projection retain observed original semantics.
Original Badlands reverse data produce length44.1452 versus forward5037.77 due to
their accumulated-distance values; no speculative correction is applied.
Version3 is tested with a synthetic loader-layout fixture; installed resources
are version4. Original route editor/recording/persistence and exact SIMD distance
approximation are not implemented. Scalar distance uses standard C++ square root.
Mapped original functions remain partial until integrated/reference validation
establishes their complete recovered behavior.
