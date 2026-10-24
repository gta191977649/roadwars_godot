# Graphics settings

The native frontend Graphics page has Display and Quality pages. Change PAGE with
Left/Right or Enter. Changes are drafts until APPLY AND SAVE. Window/display changes
require KEEP AND SAVE within15 real seconds; Escape or timeout restores the prior
settings. Escape/BACK asks SAVE CHANGES? using the same original YES/NO bitmap dialog as Garage. NO discards and leaves; YES applies/saves and leaves (display changes still require the bounded confirmation); Escape cancels the question. Defaults do not overwrite files until applied.

Display controls include monitor, windowed/borderless fullscreen/exclusive fullscreen,
window client size, VSync and a render-frame limit. Fullscreen uses desktop monitor
size. Frame limits do not change the fixed60Hz simulation frequency.

Quality controls use RenderingServer and Godot Shader Language: FXAA/MSAA, internal
3D scale, world-texture downsampling and anisotropic filtering. HUD/font textures keep
their original resolution and sampling. Chrome uses the original linear lookup and
vertex formula; the authorized16cr1.bmp compatibility asset remains the default.
View distance, effects and skid sampling are applied at the next scene construction.
World texture/viewport controls apply when confirmed. CAR_DETAIL uses recovered
vehicle body distance thresholds, shared by racing and frontend previews.

## Rendering backend

Windows can select Vulkan or DirectX12; macOS can select Metal or a compiled Vulkan
backend; Linux can select Vulkan. Auto chooses Metal on macOS and Vulkan elsewhere.
Only APIs advertised by the actual engine build are offered. Saving a different API
launches a bounded, owned GPU probe using the same engine and a rendered shader
fixture. Failure preserves the previous configuration. Backend changes take effect
at restart; the page shows the actual current API separately from the saved choice.
The renderer stays Forward+; no Compatibility/OpenGL pipeline is offered.

All GPU shaders use Godot Shader Language. Godot owns conversion to the selected
platform API; there are no backend-specific GLSL/HLSL/Metal shader copies.

Confirmed preferences are written atomically to user://graphics.cfg. This is also
the project's startup ProjectSettings override, read before graphics-device creation.
The preceding file is preserved as user://graphics.last-good.cfg. Original installation
files and developer config.local.json are never changed.

Recovery: start with an explicit working engine argument --rendering-driver vulkan
(or metal on macOS), followed by user arguments -- --reset-graphics. This removes
saved preferences after the working driver starts. Explicit engine driver arguments
override the saved startup driver for that run. Restart from the settings page removes
session driver overrides and retains user/game-directory arguments.

## Validation

python scripts/test_graphics.py runs reconstructed GPU settings tests and a second
process verifying that the saved driver is selected during engine startup. Add
--assets to check garage chrome settings and scene teardown. The harness backs up
and restores user graphics files even when a child fails. Native backend support
requires separate platform validation; Windows results do not establish macOS/Linux
runtime compatibility. Original RoadWars is not launched by these checks.

## Original frontend presentation (2026-10-24)

Graphics now uses the original options background, title/body bitmap fonts at native size, vertically centered rows and selection panel. Available editable rows display left/right arrow glyphs even when the modern host supplies only the current value. Selected labels, values and arrows use the original red-to-yellow gradient; unavailable rows stay gray. Hit testing uses the same native font height and row spacing. Implementation/debug API status text is omitted from the page.

The save question overlays the existing options and reuses `FrontendCustom`'s original dialog assets, button positions, small font and green YES/NO focus color, exactly as the Garage adapter does. Default focus is NO; Left/Right and pointer hit testing select the buttons. Failed saves retain the draft and show the error. Original graphics hardware rows remain modern backend adaptations; this presentation correction does not claim new binary findings or original-runtime equivalence.

Validation: portable graphics menu tests cover draft editing, display timeout, save confirmation, ESC cancellation, NO/discard and YES/save-and-leave. `python scripts/test_graphics.py --assets` exercises native Vulkan/D3D12, quality, chrome, Graphics display/quality captures and the original save-question overlay, restoring the user's graphics preferences afterward.
