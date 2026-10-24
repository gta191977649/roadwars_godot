# Garage UI reconstruction

Reference executable SHA256:
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
This pass corrects original UI parameters using disassembly, original resources
and installed-asset reconstruction tests. It does not claim original-executable
runtime pixel equivalence.

## Restored layout and typography

| Region | Original evidence | Restored behavior |
|---|---|---|
| Six-category menu | `00438393`, `0044000a`, `0043b762` | Font1, spacing 2, scale 1; configured selection origin and spacing; top glyph vertices offset +5 X, lower vertices unchanged |
| Store item, quality, action and arrows | `00443569` | Nebula12, spacing 1, configured scale and scaled font-height row spacing; selected gradient also applies to arrows; absolute right-edge arrow placement |
| Description and advice | `00443569`, `00460d57` | Nebula12 and configured description scale/colors; native character-scanned wrap, leading-space handling and word rewind; no invented bottom clipping |
| Mechanic image | `0044a0f0`, `004455bb`, `0043a208` | Store's actual 16Mechanic.bmp, 128x128, interface opacity; native cached advice-Y offset is preserved rather than using the unused mechanic-Y setting |
| Status panels | `0043d79c` | Original panel origins, Nebula12, both-axis centered name/cash, grouped cash; localized NITROUS and BOOST labels; actual conditional equipment and ammunition |
| Vehicle icons and damage | `00438393`, `0043d2d0` | Restored vehicle-specific body icons and five health-tinted atlas regions, original source bounds, offsets and independently truncated 0.84-scale destination edges |
| Equipment status bars | `0043d79c` | Actual 16Meter.bmp source region (0,1)-(25,6), stretched and tinted red/green at interface opacity |
| Prices and labels | `0045fee8`, `0044a0f0` | Comma-grouped action prices, native ungrouped trade-in footer, conditional trade/ammo rows; translated labels never become asset paths; descriptions use GarageDescriptions before ammunition substitution |

Font measurement and glyph placement share native per-byte integer advances.
Spaces omit letter spacing. Centering uses integer half-width and truncated
half-height. Fractional scales and unsupported glyph advances are tested against
independently decoded original font data.

## Dialogs

Confirmation (`004afd70`) uses the original 608x124 panel at (16,178), opacity
0.6, and two 108x48 buttons at (207,233) and (325,233). The message anchor is
(320,217); button anchors are (261,257) and (379,257). Text centers on both axes.
No is selected initially; selected text is green.

Insufficient funds invokes the separate original notice (`004b090a`). It uses
an opaque panel and one centered OK button, with the original green gradient.
The former extra full-screen dark mask and arbitrary 400x170 panel are removed.
Unavailable products retain the original sound-only rejection path.

Both dialogs retain their preceding Garage commands and freeze scene/board
time, including during chained warnings. Native Escape is ignored; confirmation
requires an explicit choice and accept. Shared portable geometry supplies both
rendering and hit testing. Pointer support remains a reconstruction extension;
the original modal routine polls keyboard/joystick. Modern graphics settings
keeps its separately authorized Back/cancel behavior.

## Validation

- Core and Godot extension builds passed.
- Core CTest passed 71 executed tests; the default installed catalog test was
  skipped. Installed frontend custom/menu tests were separately executed and
  passed, including all twelve character UI resources and all configured store
  items/qualities, font metrics, atlas UVs, fixed positions, colors, dialog
  geometry, frozen commands, dismissal and cancellation.
- `python scripts/smoke_frontend.py --garage-ui` has headless and native modes.
  The native test generates 88 captures at 640x480 and 960x540, covering all six
  stores, main selections, every visible row, changed products, Yes/No, notice,
  sequential warnings, ignored Escape, Summary reentry and resource reloads.
- Native captures were inspected for the store, modal and warning layouts.
  Original-executable captures were not available; screenshots validate the
  reconstruction's output and recovered parameters, not a direct original
  framebuffer comparison. Native screenshots currently use the first Single
  character; all twelve characters are covered by portable drawing tests.
- The existing native Single/Quick race-menu regression passed, including actual
  equipment effects, restart/return, continuous character music and independently
  measured purchase/door mixer output. Both Godot adapter CTests passed.
- `python scripts/test_graphics.py --assets` passed the existing GPU settings,
  startup, quality, chrome asset and shared graphics dialog/menu regressions,
  restoring saved user graphics preferences afterward. Progress update/check,
  portable boundaries and `git diff --check` passed.

Local logs/captures are ignored build artifacts: `garage_ui_ctest.log`,
`garage_ui_custom_test.log`, `garage_ui_menu_test.log`, `garage_ui_headless.log`,
`garage_ui_native.log`, and `frontend_captures/garage_ui_*.png`.
Address-linked raw evidence and remaining rasterization/localization limits are
in `docs/agent/evidence/garage_ui_fonts.md` and `garage_dialog_ui.md`.
