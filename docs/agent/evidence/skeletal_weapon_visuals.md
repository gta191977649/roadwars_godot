# Skeletal weapon geometry and jammer visual ownership

Binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only bodies/listings are retained in skeletal_weapon_visuals_ghidra.json.
No original C++ class/source identity is claimed for SkeletalMeshRuntime.

00433867 loads Electron Cannon Lightning.ska and LightningAnim.ska, calls004ee649
with animation0/mode2 (loop), and sets flags0xf. Mesh004eee40 consists of u32
vertexcount, vertices of xyz/normal/uv (eight float32,32bytes), u32 trianglecount,
three u16 indices per triangle,64byte texture name, then recursive bones004eef32.
A bone stores u32 vertex count, that many u16 vertex indices, then u32 child count
followed recursively by children.004ef02b/004ef180 flatten bones in preorder.

Animation004ee4ac stores u32 frame count, u32 frames-per-second, then a bone-major
array of frame-count 4x4 float32 matrices (64bytes each).004ee781 selects each
bone's matrix at bone*framecount+frame.004ee7fc and004ef50d transform the ORIGINAL
listed vertex position/normal directly with that bone's matrix. Parent transforms
are not composed. Duplicate vertex assignments are overwritten in preorder.
UVs are copied unchanged; normal transformation excludes translation and has no
normalization. Undefined native unassigned vertices are rejected explicitly.
Native matrix helper004d74e3(dest,A,B) computes B*A. Skeletal draw's Y/X/Z/scale
calls therefore yield Scale*Z*X*Y; the portable original_skeletal_transform preserves
local-axis scaling and native Euler order (different from a naive argument-order
matrix product). Electron supplies its actual negative owner pitch to this helper.

004ee649 uses1/fps (005f5118=1).004ee6b5 increments elapsed and advances at most
ONE frame when elapsed>=period, resetting elapsed to0 (discarded remainder),
then wraps in mode2. The coordinator advances only while Electron active, before
its collision update. Animation phase is not reset on each activation.00433b53
and004ee888 use width,height,beamlength diagonal scale.00433f1b copies owner yaw
and NEGATIVE owner pitch into beam Euler fields and actual mounted origin.
Read-only geometry and scaled pose are exposed for platform rendering.

004ee8c6 flags0xf disable Zwrite, enable SRCALPHA/ONE blend, disable lighting,
and disable fog while drawing. Vertex format0x112 is XYZ,NORMAL,TEX1. Original
texture resolution uses Cars/Shared/Textures/. The platform owns dynamic buffers
and material/RID lifetime; portable code owns the animation and vertex positions.

Jammer00465217 initially copies owner position/yaw/pitch/roll.00465395 then copies
owner position/pitch/roll every callback, but its random yaw reads the EFFECT mesh's
OWN previous heading (assembly004654aa..b3 and004654ea..f0, ECX=weapon+2dc).
The +/-rand()%270 delta accumulates; it is not relative to owner's current yaw.
Its opaque grey color uses the separate first rand()%10 draw. VehicleCombat exposes
actual pose/visibility/color_argb, preserving the final inactive cleanup callback.

Executed tests: warning-enabled standalone skeletal_mesh_runtime_test passes mesh
layout, bone-major matrices, direct child overwrite (no parent composition), normal
translation exclusion, UV preservation, frame threshold/wrap/remainder dropping,
reset, malformed/truncated/trailing input, and transactional failure. The optional
VehicleCombat original-asset test loads real Electron SKA data for every installed
car. These are portable static-reference contracts, not original pixel/runtime
comparison or complete native renderer equivalence.

