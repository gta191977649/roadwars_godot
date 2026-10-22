# Collision body texture and deformation

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static analysis only; no original executable was run. Original class/source
ownership remains unresolved. `VehicleDamageVisuals` is a portable state owner.

## Entry and ordering

`004215fd` takes its receiver through vehicle+154, a six-float force/contact record
and a health-enabled flag. It first rejects invulnerability (`00414cb2`) and any
nonzero dying timer+134. Severity=(1/mass+5944)*float005f5304*force modulus,
where005f5304=.015978554263710976. Severity<.029999999329447746 rejects the entire
function. Collision local point subtracts vehicle origin then uses normalized
D3DRMVectorRotate(-heading*.017455555498600006), then scales by the saved modulus.
The force uses the same normalized rotation but does not restore its modulus.

Health `004211a0` runs first when enabled, using player difficulty scaling only on
that health argument. The visual severity remains unscaled. The entry dying check
must not be repeated after a lethal health call, or marks and random draws would
be omitted. The receiver's Battering Spikes/Hydraulic Ram suppress visual changes
for a front contact angle<60 degrees, but do not suppress the preceding health
dispatch. `004211a0`'s duplicated negative-X predicates are confirmed by assembly
004211b8..0042122a and004212af..0042131b: armor3 handles that region first and jumps
out, so the identical armor1 branch is unreachable. This is not corrected here.

Wall `0042e8f0` also invokes004215fd, after physical response and sparks, only on
its first solver iteration. It supplies response force*.75 and actual contact
point, health-enabled1. Assembly0042ef84..0042efb1 confirms this call; the preceding
speed>5 predicate gates0041553d, not this damage call.

## Marks and shared random stream

All current base body vertices in native part/vertex order are scanned, including
untextured parts. Strict XZ bounds are local point plus/minus .32499998807907104;
height is ignored. Side storage distinguishes front0, rear1, negative-X2 and
positive-X3 using width/length and the native .33 longitudinal split. If an
out-of-body vertex matches no region, original local side is uninitialized or
retains the preceding vertex's value; the portable optional side preserves that
uncertainty without affecting known texture/RNG operations.

`004224c0` accepts at most100 marks, independently of deformation count. Each new
mark creates four rectangles, each consuming two shared CRT draws in order:
centerX=ftol(U*255)-5+rand()%11, centerY=ftol(V*255)-5+rand()%11. Edges center+/-4
are clipped only left/top to0 and right/bottom to255. Four same-source/destination
rectangle DirectDraw Blt calls use flags0, so they copy Damage.bmp pixels directly
without alpha/key blending. Invalid or empty rectangles leave pixels unchanged;
their random draws and mark records remain consumed. Once100 marks are stored,
further calls consume no draws.

Assembly005167d5 uses FISTP **QWORD**, with truncation control mode. Its64-bit
integer-indefinite for NaN/out-of-range has low EAX0. Valid64-bit values are reduced
to low32 by the caller; following additions wrap as x86 integer arithmetic. Raw
MSH UVs are retained separately from sanitized render UVs. Thus Sarge's untextured
NaNs are handled by this native conversion, not a C++ undefined floating cast.

## Mesh and bitmap state

`00420935` resolves [DAMAGE] BODYTEXTURE from Cars/<vehicle>/Data/Data.ini and
selected paint directory, replacing the final .bmp with Damage.bmp for the
source. Bitmaps use the existing original texture preparation (native bottom-up
surface orientation and1555 quantization for16-prefixed assets). Current recovered
assets are256x256; unsupported sizes fail explicitly. Rectangle right/bottom are
exclusive, preserving the native255 cap rather than expanding to256.

The paint surface is shared by canonical full texture path across vehicles.
`004ea922` checks004eca53 on the uppercase joined path; a cache hit assigns
manager+13830[index] to the material and AddRefs that same surface. `00420935`
queries004ec8a5 on that same global texture manager (root+998), with no clone.
Thus two vehicles of the same model and paint share damaged pixels, although
their100mark and4deformation limits remain separate. The portable shared PaintState
and share_paint_with adapter reproduce that identity; the scene owner connects
equal canonical paths after loading. Rendering must also reuse their mutable
texture handle. Shared paint revision plus private deformation revision is new
infrastructure to invalidate read views. This deliberately retains native cross-
vehicle paint changes rather than inventing isolated per-vehicle paint surfaces.

`0040f06f` appends SpatialObject2,3,4,5 as body LODs. `0042299b` loops exactly four
meshes: base1024Body (fallback Body), then the first three appended LOD roots.
The fifth LOD is unchanged. At most four deformation records are applied. Current
vertices strictly inside the same XZ rectangle receive displacement
(rotatedForceX*min(severity,.25),min(severity,.25)/5,
rotatedForceZ*min(severity,.25)). The kernel never recalculates normals;004d26f0
copies the changed native vertex arrays to renderer buffers. UVs/indices/colors
and attached child objects are unchanged. Repeated marks continue after the fourth
deformation. Read-only mesh/bitmap views and revision expose changes to rendering;
revision is new infrastructure, not original state.

## Verification and limits

Warning-enabled standalone C++20 build and synthetic test passed, covering severity
and front guard gates, exact8 draws/mark,100mark limit,4deformation limit,1555-ready
bitmap rectangle copies, unchanged normals, all4LOD mutation and NaN conversion.
All twelve installed vehicle models and all six paint jobs each successfully
loaded their actual4LOD meshes and paint/damage bitmaps, then applied a collision
at an actual body vertex with exact8draws per recorded mark. The synthetic test
also verifies shared same-path paint, isolated different-path paint, and private
deformation/mark ownership. Full runtime parity is not claimed.
Saved garage repaint/network replay remain outside the fresh diagnostic scope;
the live caller owns health/force modifiers and original collision phase ordering.
