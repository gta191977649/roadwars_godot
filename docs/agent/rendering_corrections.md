# Rendering and driving reconstruction integration

Use the binary identity and read-only records in evidence/ before changing any
recovered formula. Source paths/classes remain unresolved; asset directories
are not original C++ ownership evidence.

| Area | Portable behavior | Godot ownership | Evidence |
|---|---|---|---|
| Coordinates | Original positions, yaw and vehicle physics | Reflect Z at adapter boundary; camera basis conversion | evidence/steering_audit.md; coordinate_conversion_test |
| Textures | Original file-row order, X index0 key, 16-bit quantization, XX companion alpha | Texture RID and original alpha/depth/blend shader modes | evidence/render_texture.md |
| Track | Configuration, regional view distance, sky geometry/UV and scale | Native Sky, Environment, directional light and mesh instances | evidence/track_render_pipeline.md |
| Billboards | Original flag-0x40 yaw-only atlas/face convention | GPU inverse-view basis; static instance transforms; radial culling bounds | evidence/billboard.md |
| Fog | Track RGB, current regional far distance, start.7/end1, object exemption | Custom vertex-linear FOG output in native shaders | evidence/fog.md |
| Wheels | Attachment, suspension, side mirror, steering and spin | Instance transforms | evidence/wheel_render_pose.md |
| Particles | Original spark/tire gates, pools, lifetime, motion and atlas | Native particle RIDs with portable slot states | evidence/particle_effects.md |
| Skids | Five rolling strips per wheel, normalized rotated direction and original sampling | Native mesh/material/instance RIDs | evidence/skid_marks.md |
| Side weapon mounts | Initially hidden ULRORMount1..3 subtrees | Visibility respected before mesh upload | evidence/weapon_mount_initialization.md |
| Transmission/grip | Torque, traction limits, RPM, auto gears, keyboard reverse and dt | Input adapter only; no Godot vehicle physics | evidence/transmission.md |
| Camera | Recovered active chase and baseline obstruction; separate new mouse orbit | Input events and RenderingServer camera | evidence/original_camera.md |
| Engine sound | Original PCM decoding, pitch, gain, cadence and audibility | Native AudioStreamWAV/AudioStreamPlayer | evidence/engine_audio.md |
| Tire sound | Original material sample, grip gate, rising edge and loop stop | Native looping voice | evidence/tire_audio.md |
| Impact sound | Wall/landing and car/car threshold contracts; stock wall/landing hooks | Independent native one-shot voices | evidence/collision_audio.md |

Godot uses Forward+ with Vulkan on Windows. See evidence/vulkan_migration.md for
real-GPU acceptance and coverage. No per-car MeshInstance3D/vehicle body is
introduced. All native handles/voices are released on exit and restart; geometry
and textures are cached, while moving instances and dirty skid strips update.

Remaining limits include full LOD/hierarchy/render-queue equivalence, environment
mapping, legacy gamma/driver quantization, damaged/weapon effects, original random
call ordering, all camera modes, orbit-specific collision and QMDX spatial audio.
Back-face culling uses Godot's clockwise front convention after coordinate
conversion. Original initialization has no observed explicit CULLMODE assignment;
legacy device-default inference is not a captured original-driver result.

Full initialization0045c8ab and race dispatch are partial. The explicit free-driving
profile selects recovered subsets; unsupported strict calls retain trace/assert.
No original-runtime differential capture was performed and no function is marked
verified merely because reconstructed tests pass. See evidence/rendering_validation.md.
