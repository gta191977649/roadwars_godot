# Race equipment HUD

Reference SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Original class/source names remain unresolved. These are additional methods of
the existing portable RaceFrontend, not a claimed original C++ class identity.

`00487fbe` loads actual selected weapon16InterfaceIcon.bmp files from each slot's
weapon resource directory, the shared Nitrous Boost icon, shared Damage background,
and car-specific16DamageShellInternal.bmp. All use the original128x128 panel at
(0,352)..(128,480). Damage crop rectangles and X offsets come from each car's
Damage/Data.ini. Its initial selected panel is1 (damage), switched to0 (map) when
the car's cockpit flag+4ab0 is nonzero. Fresh0040d4cb construction sets thatflag1,
so the actual fresh Single Race default is map0. Cycle rules belong to
race_ai_combat evidence.

`0048c313` draws damage background at alpha0.7. Internal and four armor regions
use clamped current/maximum ratios. Above0.5, red=trunc((1-ratio)/0.5*255), green255;
at or below0.5, red255, green=trunc(ratio*2*255); blue0. The crop stays unchanged
and its destination shifts by the configured internal/shell X offset. Assembly
at0048c3a7..0048c40c repairs the decompiler's dropped color-call arguments.

`0048c90c` selects weapon icon and mount ammunition. Locked weapons have RGB100;
unlocked weapons have RGB255. The HUD uses lock<=race_time, distinct from the
strict activation comparison. `0048cb23` instead dims Nitrous when its actual
ammo==0. Both center the decimal ammunition at panel center(64,416), with text
alpha178/255 and uniform vertex color. These decisions do not use guessed cooldowns.

All layout, font selection and quad construction remain portable. The Godot
adapter receives existing HudQuad data only. Original cockpit view, time/result
messages and arbitrary localization are outside this focused equipment display.
The factory/activation/system completion status is tracked separately.

Retained raw queries: race_equipment_hud_ghidra.json. Original-asset assertions in
race_frontend_test verify Animal crop destinations, ratio colors, locked/unlocked
ammo display, missing/invalid selection errors and texture teardown. These do not
establish original-executable or pixel equivalence.

## Terminal vehicle HUD

`0048cf43` bypasses normal gauges/equipment after completed laps or nonzero
vehicle+134 dying timer and calls the map unconditionally. A dying selected
player additionally draws source-key CAR ELIMINATED centered at(320,400), with
ARGB e5e50000/e5e5e500 top/bottom colors. `00487fbe` obtains this key through
Localization/Misc.txt. The current English HUD emits that source key; arbitrary
language selection and the transient terminal result table remain unverified.
Retained queries: race_terminal_hud_ghidra.json. Original-asset HUD tests assert
forced map, suppressed equipment/gauges, eliminated colors and finished map.

## Independent weapon lock warning

The race-loop overlay is documented in weapon_lock_warning.md. It precedes the
main HUD, uses the actual English key WEAPONS LOCKED, and remains visible when
ordinary HUD elements are hidden. Its countdown/time predicate is separate from
the equipment icon brightness predicate. Native space advances omit the extra
two-pixel nonspace spacing; the portable text helper and centering now agree.
