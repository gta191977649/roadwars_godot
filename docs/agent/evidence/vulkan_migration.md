# Vulkan Forward+ migration

The default desktop rendering method is Forward+; Windows explicitly selects
Vulkan instead of relying on the engine's platform driver default. The mobile
method override selects Mobile (not Compatibility); mobile hardware is untested.
RenderingServer retains ownership of geometry, materials, skies, particles and
canvas resources. No direct RenderingDevice submission or simulation changes
are needed. This is new integration policy, not recovered executable behavior;
no original function statuses or evidence claims are advanced.

Godot's RenderingDevice architecture renders Forward+ 3D in HDR linear space:
https://docs.godotengine.org/en/4.7/engine_details/architecture/internal_rendering_architecture.html
The existing source-color texture/uniform hints, explicit prelit vertex-color
decode, custom vertex-linear fog and native sky/particle shaders compiled and
passed the GPU checks without changes to recovered formulas.

## Validation (2026-10-22)

Godot 4.7.2, Vulkan 1.4.359, Forward+, Intel Arc B390 GPU.

- Portable core and Godot extension builds succeeded; all 29 core CTest tests passed.
- `python scripts/smoke_godot.py`: 33,282 triangle pixels, ten restarts, no retained RIDs.
- `python scripts/smoke_godot.py --fog`: four fog depths and exempt materials passed.
- `python scripts/smoke_race.py --preview`: original sky, asset upload and three restarts passed.
- `python scripts/smoke_billboard.py`: 24 views, winding, atlas orientation and GPU alignment passed.
- `python scripts/smoke_race.py --particles`: 36,586 differing pixels, sparks/tire smoke and teardown passed.
- `python scripts/smoke_race.py --frontend`: HUD, map motion/reset, resize and three restarts passed.
- `python scripts/smoke_race.py`: acceleration/steering/braking/reset and native skid pixels passed.

The preview capture was visually inspected for terrain, vehicle, sky, transparent
sprites and HUD. Captures/logs remain under ignored `build/`. Render smoke runners
now reject a non-headless run unless the engine reports Vulkan Forward+ so an
automatic driver fallback cannot silently satisfy this acceptance.

These checks cover the current Dark Forest/Animal profile and synthetic fixtures.
They do not establish original-driver pixel equivalence or every track/GPU.
