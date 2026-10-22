# Battering Spikes and Hydraulic Ram

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only decompilation/constants are retained in contact_weapon_runtime_ghidra.json;
constructor vtables are retained in weapon_runtime_ghidra.json. No original-runtime
execution or source-class name recovery is claimed.

00407ec0 constructs Battering Spikes with vtable005f50b8.00455bc0 derives through
that constructor and installs Hydraulic Ram vtable005f560c. Both initialize their
model and own-vertex XYZ bounding extents through00407f9c. The model's Z extent
at weapon+254 becomes vehicle+4eb0 when the factory attaches this front weapon;
the collision extension remains the full model length throughout ram animation.

The Battering Spikes virtual+14 entry is004081e3. Ghidra's CDialog::CreateIndirect
library match at this address is demonstrably false: its constructor-installed
weapon vtable and call to004cf5ed establish game weapon behavior. Hydraulic Ram
virtual+14 is00455c26, which resets extending+538=1 and local offset+53c=0 before
calling004081e3. Historical names do not justify placing this code in CDialog.
The registry ownership audit is retained separately in weapon_ownership_audit_ghidra.json.

Vehicle collision wrapper0041dc29 (current Ghidra containing entry0041dc29) checks
that the attacking vehicle has one of these front weapons. It computes the full
3D angle from cached forward+5b10 to actual contact point minus vehicle position,
and calls the weapon's virtual+20 only when that angle is strictly less than60
degrees. The portable predicate consumes an existing pair-collision contact;
it never synthesizes proximity damage or a substitute overlap test.

Spikes virtual+20,00408213, adds normalize(existing_force)*1000 to the actual
collision force. Ram virtual+20,00455c63, first performs that operation and adds
another normalized1000 only while extending+538 remains nonzero. These functions
do not themselves call hitpoint damage. Their common004cf69c call records replay
damage state, and later vehicle physics applies the modified force. Separate
front armor resistance multipliers belong to the recovered damage code00414e7d.

Ram virtual+38,00455cd2, extends its attached weapon root's localZ at2units/sec
up to half its model length. Only strict overshoot clears the extending latch;
exactly reaching the maximum retains the extra force until the next update.
The root then retracts at2units/sec. Reaching localZ0 clears active+270 and removes
the callback. Spikes virtual+38 is an actual empty function00408206; there is no
invented timed activation or damage pulse. A zero DEFAULTAMMO for installed
Spikes is consistent with passive force modification through collisions.

ContactWeaponRuntime exposes the confirmed animation state and force modifier.
Presentation must apply localZ to the actual attached weapon mesh; the race must
call its front-angle gate and modifier from the existing pair-collision force
path. Missing geometry reports unavailable. Native callback registration,
reparenting and replay behavior remain partial and carry RE-STUB mappings.

Warning-enabled GNU C++20 compilation and contact_weapon_runtime_test passed.
Tests cover passive1000 force, ram extra1000 only while extending, exact-maximum
latch behavior, retraction/readiness, activation-before-active-gate reset,
unmodified collision extension, forward/side/rear/vertical angle gates, zero force
and invalid inputs. These are static-reference contract fixtures, not an
original-executable equivalence claim.
