# Original WEAPONS LOCKED warning

Reference SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Raw evidence: `weapon_lock_warning_ghidra.json`. This is a read-only reverse audit;
the portable HUD implementation and tests belong to the integration owner.

0047f790 copies key WEAPONS LOCKED at006196e4 into race+e28 and localizes it through
Localization/Misc.txt using engine language+8d0. The integration owner's packed
Localization/Misc.bin probe confirms the English text remains WEAPONS LOCKED.
WEAPON DISABLED is not substituted for the original English resource text.

## Exact condition and update order

The rendering block is in00480b40 at0048233e..00482446. All conditions must hold:

- Countdown race+3a8 is exactly zero at entry to this update's countdown branch.
- Constructor mode race+eec is zero.
- Updated race time+3b0 is strictly less than integer00485372's lock threshold.
- Selected vehicle's dying timer+134 is exactly zero.

There is no HUD-toggle or finished-laps predicate in this block. It executes before
the subsequent master HUD0048cf43 call, so cockpit/hidden-HUD early returns inside
that master do not suppress the warning. Finishing alive also does not independently
suppress it while time remains below the lock threshold. The selected dying timer
is the native predicate, not a newly invented HP comparison.

00480c99 first tests entry countdown<1. With nonzero dt it adds dt to race time at
00480cb5..00480cc1 before physics, global effects and rendering. Countdown is unchanged
until00482300..00482339: when nonzero it subtracts dt, clamps negative to zero and
jumps past the warning. Therefore the update that first clamps a positive countdown
to zero does not draw the warning; the next update can draw it using that update's
new race time. A portable snapshot needs the pre-update countdown-zero condition,
not only the post-update countdown. Zero-step/pause handling remains the portable
fixed-step boundary, not a reason to add extra simulation callbacks.

Race+eec is an explicit constructor mode copied by0047f790 and forwarded by004a4daf.
Single Race caller004aac8f pushes0 at004aad3a. The separate randomized-race caller
00406fc0 pushes1 at0040727f. Nonzero mode also suppresses creation of the ordinary
HUD object in00484189. Its original source identifier is unresolved; it must not
be mislabeled as the interactive HUD visibility toggle.

## Font, position and color

Assembly004823bf..00482409 selects the font at race+398 -> HUD+1bf8.00487fbe loads
this font from Fonts/16Font1.bmp and configures inter-glyph spacing2 via004e1381.
Engine+78c is the rendering surface argument, not the font pointer.

004e13b3 sets top vertex color0x66ff0000 and bottom0x66ffff00. Thus alpha102/255 is
constant; the vertical gradient runs red to yellow.004e0a26 receives x300,y5,
the localized string, flags1 and resolution-scaling argument1. Flags1 requests
horizontal centering only; the anchor is300, not the screen midpoint320. Coordinates
are in the native640x480 reference space. The block restores colors from HUD+1bfc
and HUD+1c00 afterward.

The warning itself does not set font scale. Font constructor004e0750 initializes1;
ordinary alive HUD0048bbd0 unconditionally restores the HUD font to1 while countdown
is below3, before the cockpit/HUD early return. The fresh Single Race path therefore
reaches its warning with scale1. Other arbitrary external font state is not inferred.

## Native text width and spaces

004e14dc assigns the space glyph width to first alphabet glyph width/2+1. It does
not add the configured spacing to that stored space width.004e13f7 assembly shows:

- A nonspace contributes trunc((glyph_width+spacing2)*font_scale).
- A space contributes trunc(space_width*font_scale), without spacing2.
- Finally subtract trunc(spacing2*font_scale) from the complete width.

004e0a26 subtracts integer width/2 from x300 before drawing. The final spacing
subtraction occurs even if the string ends with a space; it is not a per-character
special case. At scale1 the native metric is the sum of nonspace widths+2 and
space widths, minus2. Adding2 to spaces would miscenter WEAPONS LOCKED.

Portable original-asset tests now assert countdown-to-zero delay and strict lock
end through RaceSession, plus death suppression, HUD-hidden rendering, finished
but alive rendering, x300 centering and gradient quads through RaceFrontend.
The ordinary HUD resumes its normal colors after the independent warning pass.
No original-executable pixel comparison was run; these tests validate the retained
static contracts and do not establish full native equivalence.
