# RoadWars initial reverse-engineering assessment

Date: 2026-10-22. This report records the initial static investigation. Later bootstrap validation is documented separately; it does not validate original gameplay.

## Identity and boundaries

- Ghidra project: hp2_ps2; selected program: RoadWars; original input: the Kopio executable.
- Kopio SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
- The other installed RoadWars.exe SHA256: `51dbbc274574c26606484a2a2c4f3f6e777bb89f9a9c9e2312c8e9100e53df76`.
- Addresses below belong only to the Kopio input. No original binary was executed or modified in this investigation. Installation paths are local configuration, not portable source assumptions.

## Observations

1. `ghidra://program/RoadWars/info`: x86 LE 32-bit Windows PE, base 00400000, code section 2,048,000 bytes, reported 9,295 functions including libraries/thunks/imports. The later full inventory distinguishes 8,603 internal functions and 692 external imports.
2. Imports include DirectDrawCreateEx, DirectInputCreateA, DirectPlayLobbyCreateA, QMDX audio and timeGetTime. The observed D3DRM imports are vector math; their presence does not prove use of a retained-mode scene graph. The installed ReadMe requires DirectX 7.0. RTTI includes MFC/D3DX types; this does not establish gameplay class ownership.
3. Loose resources include 382 MSH, 380 PSE, 390 INI and 81 WAV files, plus archive contents.
4. All 102,274 bytes of Data.pkh parse into 1,814 entries: little-endian u32 name-field length including NUL, encoded path, u64 offset and u32 length. Subtracting one from non-terminating encoded name bytes yields paths. Entries are in bounds and contiguous across the 157,986,825-byte Data.pak. Extensions: 1,646 BMP, 88 D3D and 80 BIN. The beginning of the archive contains standard BMP data; not every payload was decoded.
5. Sampled Create Season and Badlands World PSE files decode by subtracting one per byte. The former has a plaintext INI counterpart. This is sample evidence, not validation of every PSE.
6. Animal vehicle configuration provides mass, drag, engine torque, gear ratios, camera and damage texture regions. Badlands World configuration includes fog, laps, collision flags, AI/map parameters and CD music timing. These parameters are not recovered simulation algorithms.
7. The LOADING TRACK string at 006199e0 leads to 00484189: track-related allocation, participant iteration, interface, force-feedback and last-lap sound setup. Object roles are inferred; layouts are not recovered.
8. The .msh reference at 0061e8f4 leads to 004d7b54. It tries the MSH path, falls back to another format and invokes binary-object reader 004d9aca.
9. 004d9aca shows recursive child objects, conditional 32/44-byte vertex allocation, six-byte triangle indices and texture loading. Multiple object/vertex formats exist; one sample layout cannot cover the entire asset set.
10. 004f0c38 shows resource-index lookup, archive reads and loose-file fallback. Decompiled parameter types remain incomplete and require disassembly corroboration.

## Engineering judgment

| Target | Initial difficulty / 10 |
| --- | ---: |
| Archive index, configuration and basic texture access | 2-3 |
| Original vehicles and tracks displayed in Godot | 4-6 |
| Playable single-player with faithful original handling | 7-8 |
| Faithful complete port including networking/replay | 9 |

The main uncertainty is original physics, collision, AI, weapons, timing and race state, rather than rendering triangles. Recover asset formats and display a vehicle/track before integrating recovered simulation. Preserve original behavior where fidelity matters; Godot vehicle defaults would introduce new behavior.

An original-EXE state bridge is an alternative research path, but no clean simulation/render boundary was demonstrated. Low-level Direct3D interception supplies draw commands rather than a complete editable scene. Existing compatibility-wrapper files in the installation were not tested. No original C++ source hierarchy is established by this assessment.

## Unresolved

Original timestep, coordinates/units beyond explicit configuration fields, complete MSH/D3D layouts, business class boundaries, original source paths, physics/collision/AI and networking behavior remain unresolved. Later C++/Godot infrastructure is separately labeled and must not be counted as recovered game code.
