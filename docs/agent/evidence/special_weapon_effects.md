# Electron Cannon and Flame Thrower portable kernels

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Static read-only Ghidra decompilation, focused assembly and vtable resolution are
retained in `special_weapon_effects_ghidra.json`. No original executable was run.
Original class/source names remain unresolved; portable types represent observed fields.

## Electron Cannon

Constructor00433710 writes vtable005f53b4 and defaults beam range+6d0=10,
width+6d4=1.5, height+6d8=1.5. Its virtual+14 resolves to00433da1, which requires
inactive+270 and ready+6c0. Common004cf5ed activation precedes clearing elapsed+6b8,
sound timer+6bc and ready, attaching the beam and configuring the original SKA.

Update00433f1b follows actual owner/mount position and heading. Active updates add
dt, subtract dt from a positive hit-sound timer, advance original beam animation,
then call00434128. The XZ ray starts at the current muzzle, ignores pitch, and ends
at heading-rotated original forward times beam range. It scans participants in
original order, excludes equal chassis identity and tests their four footprint
edges with0047c368/004dd310. First participant/first edge hit wins. It does not
sort by nearest distance or add an invented vertical/wall/destruction filter.
The impact Y is the muzzle Y; beam length becomes XZ hit distance+.1.

00434609 applies dt*weaponHITPOINTS separately to four sides in armor-index order
0,2,3,1. Each side independently uses internal damage00414d2a if its armor is
already<=0, otherwise armor damage00414e7d. Portable output uses contact-side
commands0,1,2,3, matching `damage_vehicle_contact`; it does not divide damage by4.
Hit sound is requested only when its timer<=0, after which timer becomes.5.
Elapsed strictly>1.5 deactivates after that callback's attack. The next inactive
update detaches beam, clears elapsed and sets ready only after the impact particle
pool is empty. Native SKA loading/animation and impact particle rendering remain
separate presentation work; the kernel returns hit/beam/sound commands explicitly.

## Flame Thrower

The retained initializer004353f0 resolves `295ULFire` through004d78ff, stores
the node at+2f34 and hides it through004d28b0. VehicleCombat::load now preserves
that visibility before any render submission. The persistent weapon model stays
visible; actual firing is rendered by the flame particle pool. Previously the
serialized fire sprite remained visible even while idle or in the race intro.
The installed-asset vehicle_combat test checks that this node is hidden while
the weapon root remains visible. This corrects presentation initialization and
does not establish full native constructor equivalence.

Constructor004352f0 writes vtable005f5460. Activation00435682 requires inactive
and ready+2f30, calls common activation, clears elapsed+2f2c and ready, configures
original direction and starts audio. Update0043578f first removes its callback,
rearms and clears elapsed if inactive and the flame pool is empty. Otherwise it
advances particles with actual mount position and vehicle+5b10 forward multiplied
by cached speed+10, adds dt, applies damage, then deactivates at elapsed strictly>3.
Remaining particles still move and cause damage after common firing stops until
they expire. Rearming occurs in the later empty-pool callback.

The particle kernels004ef862/004ef996/004efbee use a100-slot pool, original
AddHead/newest-first order and one-second lifetime. Emission timer+293c increases
each actual particle callback. At timer>=.1 and while firing, one particle is
emitted if capacity remains. Neither004ef862 nor004ef996 resets that timer, so
after the initial threshold emission occurs once per simulation callback. This
observed behavior is preserved on the authorized fixed simulation clock.

Creation consumes rand()%50/100 for hue and a rand()%4 atlas draw. Every surviving
particle, including a newly created one, subsequently consumes another rand()%4
atlas draw. Remaining life subtracts dt first; dead particles are removed without
animation RNG. A particle whose remaining equals1-dt skips first movement; all
others move by velocity*dt. Color uses alpha=trunc(remaining*255), red255,
green/blue=trunc((.5+hue)*255). Camera-facing vertex expansion and texture material
submission remain presentation work, not invented native equivalence.

Attack00435a93 visits every non-owner target, then flame particles newest-first,
then all four footprint edge midpoints. Strict XZ Manhattan distance<1.5 causes a
hit; Y is intentionally ignored. Edge0/1/2/3 dispatch contact sides0/3/1/2 through
00435928, which selects armor indices0/1/2/3 respectively. Every hit applies
dt*HITPOINTS to that armor or internal HP and multiplies all three components of
that particle's velocity by1-dt. Multiple nearby edges/vehicles cause repeated
damage and repeated damping in this exact order. No cooldown, single-hit deletion
or Euclidean-radius substitution is added.

## Executed validation and limits

GNU C++20 `-Wall -Wextra -Wpedantic` build and `special_weapon_effects_test` passed.
Tests cover first-ordered electron targets versus nearer targets, XZ/Y behavior,
all-four-side damage, sound interval, strict1.5-second expiry and delayed readiness;
flame creation/update RNG counts, non-reset emission timer, first movement skip,
particle order/lifetime/color/capacity, strict Manhattan hit gates, side ordering,
repeated velocity damping,3-second firing expiry and post-expiry particle drainage.
Missing shared RNG and invalid input return explicit errors. These are static-
reference kernel tests, not integrated native audio/pixel or original-runtime proof.

Adjacent contact-weapon evidence was also retained but not falsely implemented here.
In particular Ghidra's FID label for004081e3 as MFC CDialog::CreateIndirect is
contradicted by its body, which forwards to weapon activation004cf5ed. Its symbol
and inventory ownership require separate correction before treating it as library code.
