# Custom frontend presentation evidence

Reference program: `RoadWars`, x86 PE, image base `00400000`, SHA256
`93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.
Read-only decompilations are retained in `menu_custom_screens_ghidra.json`
and `menu_custom_screens_supplement_ghidra.json`.
All observations below are static; no original executable was run. Prototypes
and decompiler variable names remain uncertain. These are presentation subsets,
not complete recovery of the containing functions or their business actions.

## Confirmed screen families and ownership

| Screen family | Entry/wrapper | Custom rendering | Important variants |
| --- | --- | --- | --- |
| Graphic options | `0047700b` | `0047837d` | Original display-device/mode enumeration |
| Sound options | `00476741` | `0047837d` | CD music, music/SFX/race music, hardware and channels |
| Game options | `00475c41` | `0047837d` | User name, transmission, difficulty, cheat text |
| Keyboard/joystick | `00475168` | `0047837d` | 13 bindings; joystick adds three rows |
| Load season | `004576d2` | `004582cb` | Up to ten saved-season names, empty list |
| Create season | `00457a41` | `004582cb` | Existing names plus editable new name |
| Select session | `00472670` | `00472810` | Name and current/max players, searching/empty states |
| Start race summary | `004927c0` | `00494a1d` | Single/season/multiplayer host/join |
| Race results | `004aaae7` | `0048fd6d` | Sorted racers, DNF, points, money, animated rows |
| Standings | `004a64c0` | `004a682a` | Points/money, next track, races remaining, network |
| Season results | `004a667b` | `004a682a` | Standings body with return disabled |
| Confirmation dialog | `004afd70` | Same | Localized question and yes/no |
| Loading | `004aa5f3` | Same | Zero/intermediate/100 percent; audio fade branches |
| Pause menu | `004ac5a6` | Same | Single/season/multiplayer, race-over, no-replay variants |
| Host or join | `00451f20` | `00452e80` | IPX, TCP-IP address, serial port/baud, modem/phone |
| Garage | `00438393` initialization | `0043d79c`, `0044000a`, `00443569` | Store, tires, drivetrain, weapons, armor, repair, paint |
| Victory/defeat room | `004c9db0` | Same | Standings/camera/vehicle/3D money presentation |

`004927c0` is the summary wrapper, **not** the garage renderer. Asset directories
do not establish original source-file or class ownership. All reconstructed code
therefore remains in `game_src/unresolved`.

## Recovered 2D layouts

- `0047837d` uses `Fonts/16Font18Imp.bmp`, glyph spacing 2. Rows have font height
  plus 5 pixels; their total extent is vertically centered at 240. Label x=30;
  values are centered at x=430, with arrows x=310/610. The panel is
  `Game Options/Options/16SelectionBackground.bmp`, alpha .15, x=20 to
  610+width(`>`), and ten pixels above/below the rows. Selected labels use top
  `ffff0000`/bottom `ffffff00`; other rows are white. The title is centered at
  (320,10), return footer at (320,455). Fade duration .3 seconds; input polling
  is gated at .05 seconds. Keyboard rebinding adds an enter-to-change hint.
- `0047700b` compares view distance against floats at `005f5368=.4`,
  `005f501c=.5`, `005f5230=.75`, `005f5118=1`. Effect/skid levels compare
  `005f52f0=.33`, `005f5818=.66`, and 1. These constants were independently read
  through Ghidra memory. Original graphics rows include legacy display hardware;
  portable defaults explicitly show that device list as unavailable.
- `004582cb` draws season lists numbered at x=200, names x=230, starting at y=90
  with font-height+8 spacing. The selected row uses the red/yellow gradient.
  Empty lists display `NO SEASONS HAVE BEEN SAVED` centered at (320,220).
  The return hint is at (320,466). New-name editing uses `~` as a caret and has
  a 22-character cursor limit; persistence/deletion is outside the requested scope.
- `00472810` uses the same list positions. Session labels are
  `name [current/max]`. It has searching/no-sessions messages and a five-second
  enumeration refresh. The portable presentation accepts explicit rows; it does
  not enumerate or join network sessions.
- `0048fd6d` places 32x32 standings portraits at x=140, y=90+64*rank. Each row
  enters from x=641 at 1000 logical pixels/second; the next row starts after the
  preceding row reaches x=140. Rank/name/points/cash appear only after arrival.
  Column labels are at (370,70)/(470,70). Cash values are formatted separately
  in the original. Its glass row extends to x=590 with alpha .6.
- `004a682a` places portraits at x=32,y=90+rank*(portrait-height+4), with rank
  left of the portrait, name four pixels right, points x=250 and cash
  x=280+width(`TOTAL PTS.`). Optional next-track and races-left labels start at
  (67,360)/(67,400). Standings/season results use the same presentation.
- Summary/result/standings wrappers construct selection icons with baked label
  coordinates (70,18), scale .7. Their renderer uses icon half-size 72 for the
  selected item and 36 otherwise. A single continue icon centers at (540,392);
  summary garage/start icons center at (98,392)/(540,392). Selected overlay
  alpha runs between 0 and .7. Label compositing is not yet bit-identical to the
  original render-to-surface path.
- `00494a1d` loads `Tracks/<track>/16RaceSummaryBackground.bmp`. Racer portraits
  start at (71,148); paired columns differ by 290 pixels and each subsequent
  pair by 55 pixels vertically. Points-box offset is (+46,+1); player names are
  centered at (+156,+2) in the non-season branch. The original uses additional
  Cyber16/Cyber14/Score12 fonts and many season/network branches.
- `004afd70` centers `Screens/16DialogBackground.bmp` (608x124 at the sampled
  assets) on (320,240), alpha .6. Question y is panel-top+39. Two 108x48 buttons
  start at x=315-width and x=325, y=panel-top+55. Selected text is green; the
  original starts with No selected. Background capture belongs to the caller.
- `004aa5f3` draws `LOADING` at (41,352), an optional centered stage label five
  pixels above y=454, and a bar x=39,y=454,height=8. Its end is
  `39+trunc(percent/100*563)`; `005f59b8` contains float563. Endpoint fade is
  .3 seconds. Percentage is explicit input, never simulated loading success.
- `004ac5a6` captures the underlying framebuffer and overlays `Game Menu/16Fade`
  using `Game Menu/Screen Setup.ini` opacity. Its 512x212 panel is centered;
  selection labels begin at panel-left+BORDERINDENTX, with centered overall row
  height and spacing from configuration. The original selection font is
  `Fonts/16Font1.bmp`, spacing from configuration. Variant lists use
  `Game Menu/<variant>.ini`; no race action follows presentation in this task.

## Known implementation limits

`FrontendCustom` presents 19 custom screen IDs: options and binding values,
list pages, summary, results, standings, dialogs, loading, pause, host/join,
garage/store overlays and victory/defeat overlays. Preview racer/season/session data
are explicit presentation inputs and are labeled on screen. They do not establish
actual race, filesystem, or network state. Options remain in memory.

Original DirectDraw device and resolution enumeration,
full non-English custom-screen labels, editable user-name/cheat/session chat
widgets, all summary network-ready indicators, exact original money
formatting, and legacy audio/device branches are not fully reconstructed by this
module. Dialogs/pause retain the preceding core canvas and freeze its 3D scene.
Garage/store and victory/defeat 3D scenes are separate work; this module supplies
their original resource panels, fonts and portrait overlays. Complete damage,
trade-in, ownership-dependent action labels and live economy data remain open.
Do not mark their functions implemented based on
these 2D drawings. The parent frontend evidence describes any subsequently added
3D implementation and validation.

## Additional confirmed layouts and validation

`00452e80` draws HOST/JOIN icons at (118,322)/(520,322), selected half-size100
and otherwise50. TCP-IP owns an IP ADDRESS field; serial owns PORT/BAUD fields;
modem owns MODEM/PHONE fields. These are variants of the same screen, not
invented separate pages. `004b36c0` sends host to multiplayer preparation and
join through session selection first. Unsupported hardware values are explicit.

Garage overlays use `ModifyVehicle/Data.ini` panel positions and opacity .9,
character-specific portrait halves and font color. `0043d79c` positions Nebula12
name/cash and equipment labels over the bottom panels. `0044000a` supplies the
six category labels; `00443569` uses the store rectangle, nested item/quality
ordering, description/advice regions and action row. Fixtures are labeled
PREVIEW DATA. No purchase changes inventory, cash or files. The draw loops do
not contain a fixed product footer prompt: `0043d143` is optional FPS,
`0043fd05` multiplayer chat, and `00460fdf` full-screen fade.

`004c9db0` supplies the victory/defeat portrait strip at y428, logical width33
and gap10, centered on320, sequentially entering from x641 at alpha .7. The
destination rectangle uses actual bitmap dimensions (sampled64x64), independently
of the logical43-pixel stride. Arrived labels use racer points. Its sampled mesh
camera dummy is CamStart_2, determining the original movement denominator.
The any-key prompt is centered vertically at240, alpha100/255.

Nine original font atlases load in the original-asset test: title, Font12Imp,
Font18Imp, Arial15, Font1, Cyber16, Cyber14, Score12 and Nebula12. Extra alphabets
come from target memory (supplement). Score12 physically contains137 slots;
the shared138-character alphabet has no final inverted-question-mark bitmap in
this atlas. That glyph remains unavailable rather than substituting an image.

Validation: strict C++20 syntax checks passed. `frontend_custom_test` with the
external installation reported CUSTOM_FRONTEND_ASSETS_OK. Its mock renderer
validates finite quads/colors, texture existence and release across 19 IDs,
connection variants and five animation times, all six store categories and each
item, and all11 available pause-menu variant files. It checks results positions
at zero/.501 seconds, bad time/progress rejection, unsupported-ID behavior and
PSE-derived option thresholds. These checks establish resource/command safety;
integrated native screenshots and lifecycle verification belong to the parent
frontend record and do not establish pixel equivalence to a running original.

Keyboard and joystick names follow all 104 cases in `00486ac0`/`00487374`.
`frontend_input_names.json` records each code, target string address and text,
bound to the target SHA256. Unsupported codes use the original empty fallback.
Dialog variants use the original localized question strings, select No initially,
and retain the preceding presentation rather than displaying an empty backdrop.
