# Initial side-weapon mount visibility

Read-only Ghidra, RoadWars SHA-256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`,
2026-10-22. Records: weapon_mount_initialization_ghidra.json.

`0040face` searches exact ULRORMount1..3 names and constructs side mount state
through004cfc3d with type1. Front and rear mounts take different branches.
For a side mount, assembly004cfd83..004cfd8c passes the mount object to004d28b0.
That helper writes zero to object+0x5c, its visibility field. `004d314d` returns
immediately when this field is zero, before traversing children. Thus the initial
side-mount geometry and its ROR1 child are absent until deployed. This is not
an arbitrary rotation or an asset-coordinate adjustment.

Animal Body/SpatialObject.msh has ULRORMount1 at local
(0.746879,0.205658,-1.238234), with12 vertices and a51-vertex ROR1 child. The prior
profile uploaded both unconditionally, displaying the unfolded weapon assembly
in the initial unarmed scene. The portable MeshObject now has runtime visibility;
stock mount initialization hides the exact three supported side-mount roots,
and presentation suppresses the entire subtree. Serialized positions are retained.

The original constructor also caches attachment heights, ownership and callback
state. Those and deployment/weapon selection remain incomplete. Its mapping and
0040face remain analyzing/RE-STUB; the single visibility helper is implemented.
Tests cover all three exact names, unchanged front/rear and unsupported fourth
mount, idempotent initialization and unchanged local coordinates. Native scene
and lifecycle checks must accompany integration; no original-runtime comparison
or full weapon restoration is claimed.
