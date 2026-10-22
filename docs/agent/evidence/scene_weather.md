# Native weather callback and shared RNG

SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`. Raw bodies/constants: scene_weather_ghidra.json.
The portable SceneWeather name is not a recovered original class identity.

Track004b7439 [DATA]WEATHER==1 constructs004d0bf0 with64 slots,extent32,actual
Meshes/snowflake model and Textures/ texture base. There are no constructor RNG
calls.004d10a9 attaches and hides each model then registers004d1999->004d15bf.
This is an actual independently registered GLOBAL callback, not a per-vehicle effect.

Grid localX=(slot%8)*4-16+.1,Y3,Z=(slot/8)*4+.3. All X/Zdirectionflags start1.
004d15bf adds dt to emission timer and invokes exactly one spawn attempt at strictly
>.014999999664723873, resetting timer0.004d1385 always consumes one rand()%64,
then scans cyclically for the first hidden slot. Full64visible still consumes the draw.

Spawn adds full3D camera movement distance/dt to localZ.004d1130 normalizes old
and current camera yaw ONCE then uses abs(old-current)/dt+current yaw, not a signed
shortest angular difference. Native004d74e3(dest,A,B) computes B*A: Y/Z/X calls
produce X(-cameraPitch)*Z(cameraRoll)*Y(leadYaw). This was confirmed directly from
its indexed multiplication, not guessed from parameter order. The transformed local
offset gains current camera position and Y+10000 for the ground query, then its Y
is replaced by actual groundheight+6, including original no-surface sentinel-99999.

A spawned slot moves in the SAME callback. Visible slots face cameraYaw+180 and
-cameraPitch, fall4.900000095367432 units/sec, drift each X/Z by +/-dt and reverse
at strict originaxis +/- .5 crossings. The direction flags persist across respawns.
Age increases dt; a slot hides at Y<ground or age>3. Previous camera position/yaw
are copied only after all slot processing. No spawn catch-up loop exists.

Native constructor and004d10a9 leave previous-camera fields uninitialized. The
portable API REQUIRES explicit seed_camera_history(actualCamera), equivalent to a
zero-delta weather callback, before a nonzero spawn. This deterministic diagnostic
initialization is explicit and does not claim original first-frame equivalence.
Further native startup evidence is required before promoting complete initialization.

Executed scene_weather_test: exact threshold, required initial history, one draw
on every full-pool attempt,64slot saturation, same-tick age/orientation, actual
no-surface sentinel, negative-delta rejection. Optional installed-asset test loads
DATA/WEATHER for every installed track and actual snowflake mesh for enabled ones.
No original runtime/pixel equivalence or complete world/weather initialization claim.

