# Architecture

## Direction of dependencies

```text
Godot bootstrap Node -> portable Runtime -> portable Renderer contract
                              |                      |
                        recovered logic      ServerRenderer adapter
                                                     |
                                            Godot RenderingServer
```

`roadwars_core` is a C++20 static library with no OS or Godot dependencies. It exposes project-owned vectors, transforms, opaque resource handles, input and result types. ResourceReader and Logger are host contracts. GameEntry retains observed scheduling in an explicit legacy-reference policy and supplies a separate user-authorized fixed-simulation policy for live gameplay. The portable SimulationClock advances at60Hz independently of rendering. The Godot adapter supplies monotonic elapsed time, honors pause/time scale and renders interpolated snapshots without feeding them back into physics. Runtime remains a diagnostic triangle fixture. The explicit free-driving profile owns ResourceStore, portable initialization, TrackCollision, VehicleDynamics and RacePresentation. Read frame_independence.md before changing scheduling;60Hz is a modern policy, not a recovered original frequency.

`roadwars_godot` owns the host lifecycle and adapts contracts. All Godot types, input calls, filesystem access, native audio playback and RID management stay below `platform/godot`. Original audio commands/PCM decoding, camera, skid sampling and physics remain portable. No renderer-owned RID crosses into the core. The extension is built for the 4.7 API and loaded by Godot 4.7.2.

## Rendering and lifetime

Default launch uses `MenuFrontend` with `GameEntry::Timing::frontend_realtime` and
the new host-only frontend mode, preserving elapsed time across page transitions.
The core owns screen routing, input edges, logical 640x480 layout, text, fades,
icon animation, Cinepak video, garage/store/award scenes and their camera animation.
`FrontendCanvas` submits opaque texture handles and quads to the existing renderer.
Godot supplies file/input access, native playback and Windows GSM codec conversion.
No page logic or animation is implemented in GDScript. Menu and audio owners release
before resources and renderer, including diagnostic fixture switches. `--diagnostic`
selects the dependency-free Runtime triangle; driving remains an explicit profile.
Business actions are unavailable, while state-dependent views use labeled preview
data. See `evidence/menu_frontend.md` and the custom/3D evidence for fidelity limits.

A single GameHost Node owns ServerRenderer followed by Runtime. Runtime is destroyed first, releasing instances before meshes. The renderer releases remaining instances/meshes, detaches its viewport camera/scenario, then frees material, shader, camera and scenario. It borrows the bootstrap viewport; it does not free the viewport itself. The host must be the sole game-world renderer for that viewport.

World geometry uses RenderingServer mesh and instance RIDs, not per-object MeshInstance3D nodes. Mesh uploads are cached under monotonically allocated opaque handles. Updates submit transforms. All current calls occur on the Godot main thread. Do not access RenderingDevice internals or introduce worker-thread submission without a separate design and synchronization policy.

The simulation retains original left-handed +Z-forward coordinates. The Godot adapter
reflects Z for vertices/normals and conjugates object transforms. The portable chase
camera currently supplies a right-handed look basis; its adapter also reverses the
right column to preserve the original screen-right convention. Never negate original
LEFT/RIGHT input to compensate for a rendering reflection. Read rendering_corrections.md.

Original-asset mode uses native Godot custom vertex-linear fog, a directional light,
vertex/prelit materials and a native Sky resource. The Sky shader evaluates the
original dome triangles and UVs rather than inventing a cubemap or atmosphere.
Original particle pools retain their trigger/lifetime/motion in portable C++.
Native RenderingServer particle resources display those slots using a particles
shader, native transparency and view-depth sorting. Godot does not advance their
original motion a second time. Spark and tire pools release before their materials.

The diagnostic material is unshaded and double-sided solely to make a dependency-free smoke triangle visible. It does not establish original lighting, winding or material behavior. Original asset and coordinate conversion requires independent evidence.

## Recovered source organization

Follow original C++ directory/module/class structure when actually recovered from symbols, strings, debug information or corroborated analysis. Do not treat resource directories as C++ source directories. Record provenance and confidence for inferred organization. If no reliable path/owner exists, use `game_src/unresolved/` with address-linked names; retain stable function IDs through later moves.

`game_src/core/` and `game_src/interfaces/` contain newly designed scaffolding. No preemptive physics/vehicle/AI hierarchy is created. Generated type scaffolds live under unresolved/types until original module paths are corroborated. Add original subdirectories only as evidence establishes them. A source file may map multiple original functions, and one original function may map multiple source files.

## Build and platform limits

Windows x64 is the initial validated host. The core builds without fetching Godot; the extension needs the pinned dependency. Linux/macOS manifest entries require their own runtime validation. Godot owns input/window processing and implements ResourceReader through FileAccess using the single configured game root. Recovered simulation does not depend on Godot physics. RacePresentation releases instances before meshes, then materials and textures; GameHost destroys vehicle/presentation before resources and renderer. Native Godot audio plays portable original-sample commands; QMDX spatial behavior has separate coverage limits. Read test_driving.md for profile boundaries and validation.

## New diagnostic extensions

The diagnostic Single Race entry owns portable RaceSession, route/AI/progression,
world attack objects and the shared original CRT stream. VehicleDynamics owns each
vehicle's actual equipment/health and VehicleCombat coordinator. Original class
names remain unresolved. New portable owners stay under unresolved with address
and object-field evidence; no Godot host type owns weapon algorithms.

RaceSession applies original equipment ordering, immediate factory initialization,
track-selected tires, player/AI command gates, shared callbacks and scene attacks.
RacePresentation consumes actual selected meshes, transforms and particle pools;
Godot adapts input, RenderingServer and PCM audio only. Diagnostic seeds and AI
counts below the original five opponents remain explicitly labeled extensions.
See evidence/single_race_combat_status.md for the current verification boundary.

`game_src/extensions/` contains the optional portable Dear ImGui/imgui-console UI
and C++ stream logging. It is new infrastructure, has no RE mappings and is built
separately from `roadwars_core`. Godot Logger callbacks, input, monitor DPI and
RenderingServer canvas/texture ownership remain in `platform/godot/extension`.
The overlay outlives scene restarts and releases its RIDs and logger at host exit.
See game_src/extensions/README.md for controls, metrics and validation.
