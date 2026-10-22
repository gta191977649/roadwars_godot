# Serialized dummy metadata and native attachment ancestry

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Raw decompilation and assembly: `mesh_attachment_ghidra.json`.

## Confirmed serialized and runtime fields

Type4 MSH records contain three12-byte vectors followed by a12-byte position.
004d9aca reads these48 bytes into stack storage. It copies only position to the
node+c0/c4/c8 and calculates heading from the normalized second vector against
global+Z into node+1f0. The first and third vectors are not copied into a runtime
rotation matrix. The+1f0 heading is separate metadata: track spawn, scene turret
and scene Electron initializers explicitly read it for their own purposes.

004d1d60 initializes node+b4/b8/bc (runtime heading/pitch/roll) to zero. Neither
the type4 branch nor the type1 geometry branch of004d9aca replaces those Euler
fields. Consequently applying the serialized dummy heading or basis to all
descendant geometry would introduce a rotation absent from the executable.

MeshObject now preserves all three vectors as serialized_dummy_basis and keeps
angles_degrees as separate zero-initialized runtime state. dummy_heading_degrees
continues to represent the native+1f0 metadata. No pitch/roll is invented from
the serialized basis. The previous VehicleCombat implementation incorrectly
applied dummy_heading_degrees to each attachment ancestor; that is removed.

## Confirmed ancestry algorithm

004d5790,004d57e0 and004d5830 start with the selected node's Euler component and
add the same component from each parent toward the root.004d3dbd forms the
row-vector rotation Rx(-pitch)*Rz(roll)*Ry(heading) from those sums, using
004fdd6d/004fdea1/004fdde7 and multiplication004fd7d2. This differs from multiplying
independent parent and child rotation matrices when more than one axis changes.

004d3ee3's live-position branch walks from selected node toward the root. Each
node's local position is rotated by its parent's cumulative Euler matrix and
added to the accumulator; root position is added last.004d53a9 confirms the matrix
vector convention. original_mesh_node_angles and original_mesh_node_pose retain
this Euler and position addition order. Their typed live-angle API does not claim
the native cached-position flag0x200 branch; actual vehicle attachment nodes in
the installed-asset profile have that flag clear.

All12 installed cars have Chassis/1024Body ancestry for FrontWeaponMount,
RearWeaponMount, ULRORMount1/ROR1/ULRORMount1Weapon and HoodPoint.0040ee2c resolves
1024Body into vehicle+449c.00429a7e reads its local pitch/roll through00428b3b and
00428b6f, advances the existing body springs, and writes through00428958 and
004289de. The portable frame therefore carries body_pitch/body_roll separately
from the chassis Euler state. They affect positions below1024Body and the summed
world angles, including mount launch pitch, muzzle flash and missile flare poses.
HoodPoint and RearWeaponMount queries use this same actual hierarchy rather than
a static local offset transformed only by the chassis basis.

## Side housing and child visual ownership

004cff1e loads ECX from mount+4, the selected ULRORMount root, and004cff21 calls
004d289b to show it on reservation.004d03d6 rotates the distinct ROR1 node's local
roll and hides the selected housing only after retraction reaches zero. The
portable asset keeps two actual subtrees: fixed selected housing with ROR1 hidden,
and a separate ROR1 subtree with its own native pose. Their visibility follows
the mount state. This restores body housing geometry previously hidden by the
stock-body renderer without replacement. If the first ROR1 belongs to a different
numbered mount, its hidden ancestor is preserved instead of showing it elsewhere.

004cf787 reads the actual mount node's cumulative heading/pitch (+264 pointer),
builds formation offsets and004d5750 stores those offsets as local ammunition
positions. Request-time body pitch participates; serialized dummy metadata does
not. Back Missile additionally has local heading180 during deployment. Native
Euler addition also governs muzzle child roll and missile flare roll; those
visual bases are not constructed by parent/child matrix multiplication.

Static type4 ancestors of scene turrets/Electron endpoints are now accepted.
Actual nonzero runtime Euler or dynamic RotObject/turret ancestors still require
their corresponding dynamic phase; those unavailable paths are not approximated.

## Tests

mesh_attachment_test passed warning-enabled direct compilation and synthetic
checks for all three serialized basis vectors, metadata/Euler separation,
noncommuting Euler ancestry, position accumulation and invalid paths. Its optional
original-asset profile passed all12 installed cars: every loaded node starts with
zero runtime Euler, all serialized basis components are finite, and all six mount,
ROR1 and HoodPoint paths retain native orientation under body pitch11/roll17 and
ROR1 roll-45. No original assets are copied into the repository.

vehicle_combat_test passed its all21 optional asset/lifecycle profile with chassis
pitch15/roll20 and body pitch11/roll17, including actual side housing resources,
housing deployment visibility and mounted missiles. race_turrets_test and
scene_electron_traps_test passed static-dummy ancestry regressions plus every
installed track. These checks establish reconstruction contracts against static
evidence; they do not constitute an original-executable render/replay comparison.
