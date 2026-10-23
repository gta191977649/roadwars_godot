# Vehicle chrome mapping

Reference: RoadWars Kopio SHA-256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra MCP observations dated 2026-10-24 are retained in
`chrome_mapping_ghidra.json`. This report distinguishes original static evidence
from implementation policy; it does not claim original-executable visual validation.

## Eligibility, texture binding and lifecycle

`004d314d` resolves the object's LOD with `004d3c2c` before drawing. Chrome is
restricted to part index zero of that resolved object, with more than two vertices
and nonzero triangles, resolved-object flag `0x400`, host field `+0x980` nonzero,
and the drawing instance's texture handle `+0x6c` nonzero. Eligibility therefore
uses the selected LOD's flags, not simply the root flags. `004d3c2c` selects LODs
using squared XZ camera distance and each candidate's threshold `+0x238`.

`004d7787` assigns the wrapper's retained texture handle recursively to spatial
descendants. `00438393` loads `Textures/cr1.bmp`, binds it to the garage vehicle,
and later releases it through `004ec621` and clears its recursive binding.
`004b7439` loads the same texture into track `+0x104` (also retained in
`track_render_pipeline_ghidra.json`). `00408e09` removes the selected player
vehicle's binding in camera mode 2, restoring track `+0x104` for other modes.
This affects the selected vehicle, not every opponent.

`004ec621` unbinds both texture stages, releases the matching cached texture and
compacts the cache after its reference count reaches zero. `004d5091` releases
and rebuilds regular and fallback vertex buffers. The latter is allocated for
chrome objects even though the live chrome draw only uses their first part.

`004db487` reads `[ROADWARS] ENVIRONMENT_MAPPING` into host `+0x980` and copies
the corresponding field from externally supplied settings. `004d63f3` leaves
the destination unchanged when the key is absent. `004dae3f` does not explicitly
initialize this field: these functions do not establish a hardcoded missing-key
default. A distribution default must be established from the original settings
resource rather than inferred from the name.

### Exact filename resolution

Memory at `0061dd14` and `00614e94` confirms the literal directory `Textures\\`
followed by `cr1.bmp`; neither literal includes a `16` prefix. `004ea597`
concatenates directory and filename and uppercases the resulting name. It tests
the existing basename prefix with `004ecbb6` (`XX`) and `004ecc3f` (`16`) only
to choose a supported RGBA pixel format: 4/4/4/4, 5/5/5/1, or generic zeros.
`004eaffb` tests available device pixel formats. These functions do not change
the requested filename or read the LOWRES option.

`004ea922` concatenates the same exact directory/name, checks its texture cache,
and probes it with `004f0c38` before any bitmap decoder is called. `004f0c38`
uppercases the key and performs the archive map lookup through `0056e9c1` and
`0056e949`, otherwise trying a loose file of that exact name. No `16` prefix
is inserted in this chain. A missing exact file returns `-9999`; the texture
loader advances through the supplied directory candidates and returns zero
if none exists. `Textures/16cr1.bmp` therefore cannot silently substitute for
`Textures/cr1.bmp` based on the examined original call chain. Installation
inventory findings and any explicit modern resource override should be recorded
separately; a similarly named asset is not proof of an original fallback.

## Vertex calculation and normal provenance

`004d9aca` reads the original lit vertices with stride `0x2c`. For chrome geometry
it copies vertex offsets `+0x0c/+0x10/+0x14` unchanged into a separate float3 array
at mesh-part `+0x0c`. This is the normal source passed to `004d5425`; there is no
normalization in that copy or the chrome calculation.

`004d5425` fetches D3D view transform 2 and world transform 1. `004d74e3`
multiplies them with the original row-vector convention to form world-view.
Only the upper 3x3 participates. For each original vertex normal:

```text
nx = n.x*M11 + n.y*M21 + n.z*M31
ny = n.x*M12 + n.y*M22 + n.z*M32
u = (nx + 1) * 0.5
v = (1 - ny) * 0.5
```

Assembly `004d5553..004d565d` confirms multiplication, addition, reverse
subtraction for V and multiplication by the constants. Memory reads establish
`005f5118=0x3f800000` (1) and `005f501c=0x3f000000` (0.5).
There is no inverse transpose, normalization, reflection-vector construction,
clamp, Fresnel factor or position dependence in these chrome coordinates.
Scale in the world matrix consequently affects chrome coordinates. Single-pass
output is vertex UV1 at offsets `0x24/0x28`, stride `0x2c`; fallback output replaces
UV0 at offsets `0x18/0x1c`, stride `0x28`. Hardware interpolates those vertex UVs.

## Material, texture stages and fog

`004d9aca` initializes D3DMATERIAL7 to zero. A textured part receives white
diffuse and ambient RGB and serialized material alpha for both colors. An
untextured part receives serialized RGBA for diffuse and ambient. Specular,
emission and power remain zero. Textured serialized RGB is therefore not an
additional tint.

`004dbbbe` sets Gouraud shading, alpha blending on, SRCALPHA/INVSRCALPHA,
lighting on, and material sources 145..148 to MATERIAL (0). It sets neither
COLORVERTEX nor NORMALIZENORMALS in this initialization. `004d2fcd` then selects
lighting, depth writes/test, fog, alpha testing and additive destination blending
from the resolved object's flags. The original light and ambient configuration
for tracks is documented separately in `track_render_pipeline.md`; garage
lighting is loaded from `[LIGHTING]` in `00438393`.

Stage 0 uses COLOROP MODULATE with TEXTURE and DIFFUSE arguments and initially
ALPHAOP MODULATE with the same arguments. Device validation can switch the latter
to SELECTARG1 DIFFUSE. Both stages explicitly set MINFILTER and MAGFILTER to LINEAR.
No ADDRESSU/ADDRESSV change occurs in the chrome draw or this initialization.
The bounded list of all `004dd230` callers is retained; this observation alone
does not prove every possible device-level sampler state change is absent.

For devices with host `+0x988 >= 2`, `004d314d` sets stage 1 COLOROP ADD (7),
TEXCOORDINDEX 1, COLORARG1 TEXTURE (2), COLORARG2 CURRENT (1), binds chrome,
draws once and disables the stage's COLOROP afterward. This adds chrome after
base diffuse lighting, with the fixed-function stage saturation before fog and
framebuffer blending. Chrome must not be multiplied by base diffuse lighting
again. Fog state itself is controlled by `004d2fcd`/`004dbeb5`.

**Alpha limitation:** initialization explicitly sets stage 1 ALPHAOP DISABLE,
and the chrome branch does not enable it. Microsoft's fixed-function
[D3DTEXTUREOP documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop)
states that disabling alpha while color is enabled is undefined. Preserving
base alpha in a modern shader is an explicit compatibility policy, not a
cross-driver behavior proved by this static code. Chrome texture alpha is not
explicitly selected by the chrome branch.

## Fallback differences and verification boundary

With fewer than two stages, `004d314d` copies/processes the regular vertices into
the fallback buffer using `004dd070`, draws the base, then binds chrome at stage
0 and selects SRCALPHA/ONE. It regenerates chrome UV0 and draws the first part
again. This retains stage 0's diffuse modulation and existing fog state; no
equivalence with the single-pass ADD path should be claimed. Lighting, alpha,
fog and framebuffer clamping can produce different results. The modern shader
can target the supported single-pass branch without emulating old capability
restrictions, but that target is a deliberate choice.

Confirmed: selected-LOD eligibility; first-part restriction; preserved raw
normal source; matrix order and numeric UV formula; single-pass RGB operations;
fallback draw/state sequence; material source colors; linear min/mag sampling;
camera-mode binding removal; configuration read lifecycle.

Not established by this static pass: physical-driver handling of disabled
stage-1 alpha; exhaustive device-state inheritance; original and reconstruction
pixel equivalence; missing-key default independent of shipped configuration.
CPU/GPU tests of the reconstruction must be reported separately from original
runtime captures and must not remove these limitations without further evidence.

## Implemented modern pipeline and explicit compatibility

The installed archive contains `Textures/16cr1.bmp`, not the literal
`Textures/cr1.bmp` requested by the reference executable, and its configuration
sets ENVIRONMENT_MAPPING=0. On2026-10-24 the user explicitly authorized using
the existing16cr1 texture and enabling chrome by default. ServerRenderer therefore
supplies `ChromeMappingOptions{true,"Textures/16cr1.bmp"}`. This is an explicit
host policy, not a recovered alias or silent fallback. The portable helper's
default options retain the literal original filename and configuration value.
An explicitly selected texture that is absent or malformed fails visibly.

The original BMP conversion preserves bottom-up upload orientation and the
16-prefix RGB1555 quantization/color key. No generated texture or guessed
environment image is substituted. The shader ignores chrome alpha and retains
base alpha as the deterministic compatibility policy for the undefined native
stage1 alpha combination described above.

`MaterialSettings::chrome_map` binds the second texture only to qualified first
parts of vehicle meshes. Both race and frontend owners retain its texture until
all referencing materials have been released. Cache keys include chrome identity,
including damaged-paint rebuilds, so nonchrome parts cannot inherit the variant.
The existing high-resolution vehicle presentation remains the selected geometry;
this work does not add a new distance-based LOD system. The predicate accepts the
flags of each submitted geometry node rather than borrowing its parent's flags.

RenderingServer uploads raw float3 normals in CUSTOM0, avoiding normalized/packed
NORMAL storage. The GPU vertex stage uses the current interpolated model-view
upper3x3 and interpolates chrome UV without per-frame CPU buffer edits. Lit chrome
uses original material-source Gouraud diffuse/ambient, separately from prelit
vertex colors. Both textures are filtered in encoded RGB; the saturated stage1
ADD follows base lighting. Vertex-linear fog is applied after that ADD, then the
result is converted for Godot's linear framebuffer. Chrome is not multiplied by
diffuse lighting a second time. The shader targets the original multistage
single-pass branch; the incompatible capability fallback is documented, not
misrepresented as equivalent. Linear/repeat sampling is the modern policy matching
the observed linear filters and the documented fixed-function wrap default;
exhaustive inherited device state remains outside the static evidence boundary.

Graphics CHROME MAPPING now changes this supported setting for the current
renderer session and subsequent scenes. It survives host scene reconstruction
without writing installation settings. Other unavailable graphics business actions
remain unavailable. `--chrome-mapping=off`/`on` provides a launch override;
`build.ps1 --run --chrome-mapping off` forwards it. Ordinary launches default on
as authorized. Returning to graphics shows the effective session value.

## Executed reconstruction validation

Portable tests cover eligibility, raw normals, rotation/nonuniform scaling,
translation independence, original/Godot coordinate conversion, RGB saturation,
missing/malformed configuration and assets, explicit resource overrides and
caller-owned texture lifetime. Installed-resource tests exercise the native
disabled configuration separately from the authorized16cr1 policy. All12 vehicle
body meshes supply eligible first-part normals, and all12 garage scenes bind and
release chrome materials successfully.

`python scripts/smoke_chrome.py` validates12 actual Forward+ shader samples across
three complete fixture lifecycles: four UV directions, wrapped nonunit normals,
vertex interpolation, object rotation, lit base modulation, encoded-RGB fog,
disabled baseline and saturation. Zero-alpha chrome samples prove base alpha
is retained. `python scripts/smoke_chrome.py --assets` checks the actual garage,
on/off captures, graphics toggle, setting survival and three fresh-scene RID
cycles. Captures and logs remain in ignored build output.

Original-executable pixel comparison is deliberately not used: the user stated
that the installed dgVoodoo translation does not support the reference effect.
These tests verify the reconstruction against static formulas/state and actual
resources; they do not establish original-driver pixel equivalence.

Core and Godot extension builds passed. The68-test core suite passed, and direct
installed-resource chrome, all-vehicle frontend and damage-visual tests passed.
The native competition smoke completed an actual race, exercised camera targets,
moving pause, restart/track changes and teardown with chrome enabled. The clean
competition startup and static original-resource race preview also passed.
These integration checks exercise damaged-paint material replacement alongside
chrome but do not assert an original-driver damaged-pixel match.
