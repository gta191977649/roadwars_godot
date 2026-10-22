# Frontend 3D presentation

Reference executable SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
The installation executable matching this hash was checked before PE constant reads. No original executable was run. Retained read-only decompilation is in [frontend_scene_ghidra.json](frontend_scene_ghidra.json).

## Confirmed presentation evidence

| Address | Observation | Portable implementation |
| --- | --- | --- |
| `00438393` | Garage and store exporter root positions are reset. Garage textures use selected character directory; lighting uses shared garage configuration. Stand anchors the car. | Room meshes, material paths, lighting, Stand and configured camera poses. |
| `0044000a` | Car root offset aligns body center 449c with Stand. Initial eye is halfway from Stand toward first camera, at Stand height and zero pitch. Camera mode 8. | Stock car/wheels, arrival pose, rotation, timed camera positions. |
| `0044bac7` | Product mesh depends on category/item/GETFROMQUALITY/quality. Product attaches to Object1. GARAGE3DOBJECT specifies initial angles and velocities. Textures use Cars/Shared/Textures. | Six store categories, visible selection changes and product rotations. |
| `004c9db0` | Room has Start and Cam1..3, CamStand targets, rotating Mount1..3, sky suffix velocities, character Picture meshes and number meshes. | Room, three preview cars/pictures, money/points glyphs, sky and turntable rotation. |
| `004ca241..004ca5cf`, `004cbee1..004cc103` | Stack identities show Start -> Cam3 -> Cam2 -> Cam1 -> Start, once. Mode 9 changes to 8 on leaving Start. Initial target is Cam1 plus normalized Cam1-minus-Start times 10. | Anchor order, mode change, target and dwell within .1 distance of requested eye. |
| `004e553d` | Mode 9 translates with per-axis difference/2 speed limiting, otherwise difference times delta acceleration; faces target after moving. | Position/angle updates. |
| `004e433a`, `004e2be0` | Mode 8 calculates look before moving, accelerates heading and blends pitch through shorter arc with coefficient 2.5 times delta. | Garage/store arrival and victory tour/return easing. |
| `0040d4cb` | Vehicle 4f10 is chassis maxY-minus-minY. | Prize row Y is anchorY plus twice height; second row adds glyph height plus .2. |
| `004d7b54`, `004d7c7b` | Prefer MSH, fall back to serialized D3D. Recursive nodes/materials/vertices/per-corner UV faces; recalculate normals and reflect X/Z. | Bounded decoder, averaged normals, expanded material surfaces. |

Hash-checked PE constants: `005f5154` float .2, `005f5158` float 2.5, `005f514c` float 2, `005f52c8` double 180. Float versus double was checked.

## Infrastructure and preview data

FrontendScene owns opaque renderer handles; releases instances before meshes and materials before textures. Failed construction unwinds resources. ResourceStore and Configuration preserve PSE precedence. No purchase, race, network or save executes. Product selection changes only visible geometry/animation; empty labels use ORDERING ITEM0.

Victory fixtures: selected character, BUD THE CRUD, NIKI ROADKILL; money 10000/5000/2500; points 10/6/4. These are synthetic display data. Glyphs and Gold/Silver/Bronze textures are original. Money has dollar prefix and comma grouping. FrontendCanvas owns 2D overlays.

Deterministic 60 Hz presentation updates are a modernization, not an original clock claim. At most 240 updates run per callback; debt is retained. Cameras use original_camera_frame with heading toward target and the existing renderer coordinate conversion.

## Verification and limits

Direct GNU C++20 build with warnings enabled and original-installation test passed: D3D boundaries/truncation/trailing bytes; original garage parse; seven garage/store variants and baseline product materials; live product replacement and invalid label rejection; victory room/cars/pictures/prize glyph uploads; initial camera direction; Start dwell boundary; garage camera switching; identical 20-second state at 30/144 fps; reload/stop; forced mesh-upload failure cleanup. Original assets and extraction output are not committed.

Parent integration reviewed corrected native captures showing garage car/room and victory room/podium cars. The initial reversed-heading issue was fixed and an orientation regression added. Final updated captures are coordinated by parent integration.

Remaining fidelity limits: dynamic garage flare behavior, original animated character behavior, race-specific equipment/damage and participants, original resource caches and full lighting fidelity remain partial. The renderer near/far range and scalar ambient interface are host presentation choices; colored ambient requires an expanded contract. No screenshot equivalence to an original runtime is claimed. All mapped original functions remain partial/analyzing; visibility does not imply full business-function recovery.
