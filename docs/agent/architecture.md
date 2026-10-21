# Architecture

## Direction of dependencies

```text
Godot bootstrap Node -> portable Runtime -> portable Renderer contract
                              |                      |
                        recovered logic      ServerRenderer adapter
                                                     |
                                            Godot RenderingServer
```

`roadwars_core` is a C++20 static library with no OS or Godot dependencies. It exposes project-owned vectors, transforms, opaque resource handles, input and result types. `ResourceReader` and `Logger` are host contracts. Runtime updates accept elapsed seconds and input; the skeleton only submits a static diagnostic triangle. The original simulation timestep and coordinate convention remain unresolved. GameEntry now applies the observed outer-loop float delta clamp and mode dispatch through address-labeled hooks.

`roadwars_godot` owns the host lifecycle and adapts contracts. All Godot types, input calls, filesystem access, future audio adapters and RID management stay below `platform/godot`. No renderer-owned RID crosses into the core. The extension is built for the 4.7 API and loaded by Godot 4.7.2.

## Rendering and lifetime

A single GameHost Node owns ServerRenderer followed by Runtime. Runtime is destroyed first, releasing instances before meshes. The renderer releases remaining instances/meshes, detaches its viewport camera/scenario, then frees material, shader, camera and scenario. It borrows the bootstrap viewport; it does not free the viewport itself. The host must be the sole game-world renderer for that viewport.

World geometry uses RenderingServer mesh and instance RIDs, not per-object MeshInstance3D nodes. Mesh uploads are cached under monotonically allocated opaque handles. Updates submit transforms. All current calls occur on the Godot main thread. Do not access RenderingDevice internals or introduce worker-thread submission without a separate design and synchronization policy.

The diagnostic material is unshaded and double-sided solely to make a dependency-free smoke triangle visible. It does not establish original lighting, winding or material behavior. Original asset and coordinate conversion requires independent evidence.

## Recovered source organization

Follow original C++ directory/module/class structure when actually recovered from symbols, strings, debug information or corroborated analysis. Do not treat resource directories as C++ source directories. Record provenance and confidence for inferred organization. If no reliable path/owner exists, use `game_src/unresolved/` with address-linked names; retain stable function IDs through later moves.

`game_src/core/` and `game_src/interfaces/` contain newly designed scaffolding. No preemptive physics/vehicle/AI hierarchy is created. Generated type scaffolds live under unresolved/types until original module paths are corroborated. Add original subdirectories only as evidence establishes them. A source file may map multiple original functions, and one original function may map multiple source files.

## Build and platform limits

Windows x64 is the initial validated host. The core builds without fetching Godot; the extension needs the pinned dependency. Linux/macOS manifest entries are provided, but those platforms require their own build and runtime validation. Godot owns input and window processing; recovered simulation must not depend on its physics engine. Host resource and audio services will be wired as recovered logic first requires them; no original-game loading is currently implemented.
