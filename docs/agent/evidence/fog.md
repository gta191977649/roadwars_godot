# Original distance fog and Godot bridge

Program RoadWars; SHA-256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only Ghidra evidence, 2026-10-22: fog_ghidra.json and
track_render_pipeline_ghidra.json.

`004b7439` selects vertex fog state140, mode3/LINEAR, normalized start.7,
end1 and track FOGR/FOGG/FOGB. `004dbeb5` multiplies these limits by the current
far distance. `004dcdd8` reapplies fog whenever projection distance changes.
`004ba95f` selects the view-distance region and global VIEWDISTANCESCALAR, so
fog must follow the actual camera position, including the new mouse orbit.
In Dark Forest at Start1, far350 means fog starts245 and ends350; RGB is96,100,80.

Object flag0x100 disables fog through004d2fcd. Sky root796 includes this flag.
The examined spark/tire particle queues and skid-strip draw explicitly disable
fog too. Applying a full-screen fog overlay would incorrectly affect them.

Godot Compatibility depth fog is not directly equivalent: its shader uses
smoothstep on length(view_position). See the official source:
https://github.com/godotengine/godot/blob/4.7-stable/drivers/gles3/shaders/scene.glsl
The bridge therefore supplies Godot's custom FOG output using vertex-computed
clamp((view_z-start)/(end-start),0,1), interpolated over each triangle. Godot still
owns shading, depth, alpha blending and the fog mix. No postprocess replacement
or external renderer is introduced. Color management remains Godot's native
linear-light path; exact legacy gamma/driver quantization is not verified.

Fog-capability fallback on unsupported original devices is outside this modern
profile. RANGEFOGENABLE is not assigned in the inspected initialization; ordinary
view-Z vertex fog is selected. The fixture checks known depths plus a second row
with fog disabled, so a radial/smoothstep substitution fails the image test.
