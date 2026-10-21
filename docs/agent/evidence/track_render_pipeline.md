# Track sky, visibility distance and render-state reconstruction

Program: RoadWars. Kopio executable SHA-256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Date: 2026-10-22. Evidence source is read-only Ghidra MCP decompilation, targeted
assembly, constant reads and original assets. No original executable was executed.
Original source paths and class ownership remain unresolved; portable helpers stay
under game_src/unresolved.

## Original track and sky setup

`004b7439` reads World/Data.txt through the original PSE-preferring configuration
loader. Rendering fields include [DATA] VIEWDISTANCE(M), FOGR, FOGG, FOGB and USESKY.
It sets ambient render state 139 to packed color 0xff636363, creates a white
directional light (type 3, yaw 45 degrees, pitch -35 argument), assigns light index
0 through `004f2606`, and enables it with `004f2637(1)`.

Light helper `004f267d` negates the pitch argument before quaternion construction,
uses float 005f5b48=0.01745329238474369 and rotates +Z through
`004fe105`/`004fef80`/`004fe01e`. Direction is approximately
(0.579228,-0.573576,0.579228). Preserve this direction as original light travel;
surface diffuse response uses the opposite incident direction.

When USESKY is nonzero, the constructor loads Tracks/<track>/Sky/Sky.d3d through
`004fcb30` and the normal spatial object loader. The existing MSH preference selects
Sky.msh. This is an actual mesh-based sky dome, not an invented cubemap.

`004fcc79`, called initially and each `004ba95f` track update, replaces the sky root
translation with camera X, zero Y, camera Z. It sets the root scale to:

```text
s = far_distance - far_distance / 20
scale = (s, s - s / 4.5, s)
```

The 20 and 4.5 values were confirmed from constant bytes at 005f5270 and 005f515c.
Ignore the serialized sky root position when applying this replacement; preserve
child-local positions. Assembly at 004b81ed-004b8200 confirms that the camera's
separate background object receives the sky root through `004d70cd`.

Dark Forest Sky.msh contains root `796sky`, one part, 91 vertices, 40 triangles and
texture 16Sky.bmp. Root position is approximately (-0.00447,0.226845,0.308596), which
the update replaces. Mesh bounds are approximately X [-1.000799,1.000799],
Y [-0.230958,0.628667], Z [-0.998323,0.998323].

## Original visibility distance and fog

`004db487` reads [ROADWARS] VIEWDISTANCESCALAR into host+0x950. The track constructor
initially applies it to VIEWDISTANCE(M), then the view-distance map supplies the
active region distance. `004cdc2c` opens World/ViewDistanceMap.bin; `004cd7b0` reads:

```text
u32 region_count
repeat region_count:
    u32 triangle_count
    triangle_count * (3 vertices * 3 float32 coordinates)
    float32 distance
```

Each loaded vertex has X and Z negated, with Y retained. `004cdd7e` initially selects
the maximum region distance. `004cde15` first tests the cached region, then scans
regions in file order using XZ triangle predicate `004d59cf`. Overlap therefore
preserves the cached region. If none match, it clears the cache and supplies 450
while issuing an original diagnostic. The portable query preserves the 450 value;
the original diagnostic dialog remains outside the helper and the mapping is partial.

`004ba95f` multiplies selected distance by VIEWDISTANCESCALAR. `004dcdd8` updates
projection and reapplies fog through `004dbeb5`. Original projection has vertical
FOV float 0x3f5f66f3 radians (50 degrees), aspect multiplier .75 for original 4:3,
and near 1. Fog render state 140 uses linear mode 3, start .7*far, end far, density
0, and the track packed RGB. This is linear fog, not an exponential approximation.
Modern viewport aspect adaptation is a host decision; the original FOV and near
distance are independently evidenced.

## Object render states

`004d2fcd` configures state from the resolved object's numeric name prefix flags:

| Flag | Original behavior |
| --- | --- |
| 0x4 | Disable lighting (render state 137); absent enables lighting |
| 0x1 | Disable Z write (14); absent enables writes |
| 0x10 | Disable Z testing (7); absent enables testing |
| 0x100 | Disable fog; absent re-enables configured fog |
| 0x80 | Alpha reference 255, compare EQUAL; absent reference 0, compare GREATER |
| 0x2 | Blend SRCALPHA/ONE; absent SRCALPHA/INVSRCALPHA |

Sky flags 796 decimal = 0x31c include unlit, no depth test and no fog. The sky is
submitted through the background path before the ordinary world, not alpha-cut
world geometry. Blanket double-sided/unshaded/0.5 alpha-scissor materials do not
reproduce these states. Alpha-test enable, material alpha and texture conversion
must also be considered; fractional alpha must not be discarded indiscriminately.

`004d314d` additionally defers flag-0x20 objects through the sorted rendering queue,
uses camera-facing rotation for flag 0x40, performs visibility and LOD selection,
and can skip children for flag 0x800. Full world hierarchy, LOD, billboard, effects,
environment-map and original culling equivalence are not established by this helper.
Do not claim the entire original renderer is verified from these isolated rules.

## Implemented helpers and checks

track_render_settings.* implements configuration, original view-distance map
decoding/query and camera-relative sky transform without Godot dependencies.
The track constructor/global configuration and diagnostic-bearing query retain
RE-STUB mappings. Numerical map decode/maximum and sky transform have RE mappings;
no function is marked original-runtime verified.

Direct GNU C++20 fixture test passes: XZ sign conversion, cached overlapping-region
priority, original 450 fallback, malformed map transactionality, sky scale/origin,
fog color, global scalar and directional-light direction. Direct original-asset
probe passes: Dark Forest has six regions, initial far 400, Start1 far 350,
VIEWDISTANCESCALAR 1, fog (96,100,80), USESKY 1. Parent integration must run its
complete build, RenderingServer image/lifecycle checks and progress synchronization.

Retained records: track_render_pipeline_ghidra.json and
object_render_states_ghidra.json. Original assets remain outside version control.
