# Original-asset free-driving profile

This profile connects the shared-data portion of initialization `0045c8ab` to
the existing Godot-driven GameEntry loop. It explicitly selects a track and Animal
without recreating the menu. `0045c8ab` remains partial and must not be marked implemented.

## Configuration and launch

`config.local.json` is the sole machine-local configuration. Its `game_dir` points to
the installation containing Data.pak, Data.pkh and Tracks. `scripts/local_config.py`
validates it; the launcher passes its value into GameHost as `--game-dir`.
The portable core receives only ResourceReader and resource-relative names.
Do not add alternate absolute paths, discovery guesses or per-tool root settings.

```powershell
.\build.ps1 --debug trace "Dark Forest"
```

The word after `trace` is a track name. The launcher validates installed track names
case-insensitively. It builds Debug and starts Godot 4.7.2; on Windows it selects the
adjacent console executable when available so failures remain visible.
W/Up accelerates, S/Down brakes, A/D or Left/Right steers, Space applies full braking,
R resets the test profile to Start1. Escape toggles the diagnostic UI mouse;
click the game background to resume game focus. Close the window to exit. The reset is a new test control,
not a claim that original lap-based recovery has been reconstructed.

## Ownership and evidence

- `resource_store.*`: PKH/PAK archive-first lookup and limited INI/PSE grammar.
  PSE is preferred over plaintext, with byte-minus-one decoding.
- `mesh_scene.*`, `bitmap.*`: bounded MSH and uncompressed BMP decoding.
  MSH flags distinguish 32/44-byte vertex layouts. Type-4 dummy heading is retained.
- `game_initialization.*`: global display/language configuration, opaque garage
  localization tables and explicit test car/track selection.
- `track_collision.*`: original surface/friction grids, Start1, wall broadphase,
  rectangular body footprint, iterative contact search and separating normal.
- `vehicle_dynamics.*`, `vehicle_input.*`, `contact_orientation.*`, `wall_response.*`:
  evidenced force helpers, keyboard steering, wheel/chassis contact and response.
- `race_presentation.*`: new portable presentation owner, static mesh upload,
  vehicle transforms, recovered-camera presentation, particles and skid strips. It does not establish an original class.
- `platform/godot`: file/input adapters and RenderingServer texture/material/mesh/
  instance RIDs. One host Node owns the game world. No Godot physics is used.

Original C++ paths/classes for these functions remain unconfirmed. Keep the files
under `game_src/unresolved`; asset paths do not establish source ownership.
Read the hash-bound evidence files for each subsystem before changing formulas.

## Coverage limits

This is an explicit undamaged, stock Street/Poor free-driving profile.
Original automatic shifting, wheel RPM, keyboard braking/reverse, stock grip,
chase camera and engine-loop playback are recovered subsets. Manual gear key
bindings, arbitrary upgrades, damage, weapons, AI, lap/race progression,
networking and menus remain outside it. Original missing-ground recovery
requires lap data and still traces/asserts at `004185fe` when reached. Unsupported
functions must retain address-linked failure; do not replace them with plausible physics.

Rendering now preserves original prelit vertex colors, lit normals, material alpha,
GPU billboard/depth/fog/blend flags, original texture row ordering, X palette-index keying
and XX separate alpha. Original sky geometry/UVs are displayed by a native Godot Sky,
with original regional view distance, 50-degree projection and linear depth fog.
Track RotObject nodes now advance their original name-encoded heading/pitch/roll
speeds in the driving profile; see evidence/track_animation.md.
Billboard sprites use the current GPU view basis; see evidence/billboard.md. Static previews
do not advance simulation. Animated collision and turret behavior remain incomplete.
Wheel attachment, right-side mirroring, steering, suspension offset and rotation use
recovered helpers. Spark and tire smoke/dust pools use native Godot particles while
original lifecycle/motion remains portable. See rendering_corrections.md for evidence
and remaining renderer/LOD/environment-map coverage. The recovered chase camera
is the baseline; mouse orbit, its three-second idle timer and reset key are new.
QMDX spatial attenuation/Doppler and orbit-specific obstruction sweeps remain incomplete.
Manual camera hold retains its world orientation while the car turns.
Mouse orbit and return use a body-pivot-centered sphere with stable target framing;
interrupting a partial return preserves the displayed composition and radius. Three idle
seconds precede the return blend; moving the mouse during return resumes control
from the currently displayed view and restarts the idle timer. Window focus loss
releases mouse capture, and scene exit restores the prior cursor mode.
Initial unarmed side-mount visibility is recovered from004cfc3d/004d28b0; do not
show the unfolded ULRORMount/ROR geometry before weapon deployment is recovered.
Read evidence/transmission.md for the RPM correction, grip/force audit and seconds
contract. Do not reduce torque or add substeps to conceal an unproven physics issue.
Only uncompressed BMP families required by this profile have been exercised.
Parent dummy transforms beyond observed simple Start1 positioning are incomplete.

## Validation commands

```powershell
python scripts/build.py core
python scripts/build.py test --preset core
python scripts/smoke_race.py --core
python scripts/build.py godot
python scripts/smoke_godot.py
python scripts/smoke_race.py --preview
python scripts/smoke_race.py --frontend
python scripts/smoke_race.py
python scripts/smoke_race.py --particles
python scripts/smoke_race.py --camera
python scripts/smoke_race.py --audio
python scripts/smoke_race.py --music
python scripts/smoke_race.py --sfx
python scripts/smoke_godot.py --fog
python scripts/progress.py update
python scripts/progress.py check
python scripts/check_boundaries.py
```

`smoke_race --preview` checks original-asset upload and three restart/exit cycles.
`smoke_race --core` runs the 20-second original-asset simulation probe without Godot,
including actual wall-contact, acceleration, steering, braking and reset assertions.
`smoke_race` drives through GameEntry and checks throttle, steering, braking and reset.
`smoke_race --particles` drives to an original impact, checks actual spark/tire state,
compares native particle visibility pixels, and restarts the complete scene three times.
`smoke_race --camera` additionally turns the car during manual hold, interrupts
return, and checks mouse capture release/restoration across focus and exit.
The driving smoke compares native skid meshes visible/hidden and checks reset;
the portable strip fixture covers normalized width, slope and ring-buffer wrap.
Headless checks do not prove pixels. Render captures stay under ignored build/.
Synthetic tests and reconstructed execution are not original-executable equivalence.

The driving/preview entry also loads the portable RaceFrontend. The original
font atlas additionally displays the new `DEV BUILD BY NURUPO` label at the bottom center only
when the launcher receives `--debug`; an ordinary trace launch omits it. This
label is diagnostic infrastructure and does not count as recovered behavior. The
tachometer, MPH readout, English gear label, Dark Forest minimap, Animal portrait
and player dot/border are rendered by a native Godot canvas below the debug UI.
The map marker uses forward Start1 and the original per-track scale, rotation and
anchor. This explicit single-car profile supplies no lap/rank progression.
`--frontend` compares shown/hidden HUD pixels, checks actual player motion/reset,
resizes the viewport and checks three restart/exit cycles. See
evidence/race_frontend.md for original addresses and remaining frontend coverage.

## Race background music

Race mode also plays the original Sfx/RaceMusic.wav fallback on an independent
native voice at the original INGAMEMUSIC volume. Portable Microsoft ADPCM decoding
and loop commands live in race_music.*; Godot only plays the resulting PCM.
The song loops across vehicle resets and releases on scene restart/exit. The
static preview has no BGM. CD transport and result music remain incomplete.
See evidence/race_music.md and `python scripts/validate_race_music.py` for the
full-sample independent codec comparison; `--music` validates native playback.

## Sliding and collision sound

`tire_audio.*` restores the first-tire material sample selection, strict grip
threshold, rising-edge latch, continuous loop and all-skids-ended stop. The
original rolling-sound channel is separate and remains incomplete. Do not make
every visible dust particle play a tire screech. Read evidence/tire_audio.md.

`collision_audio.*` restores wall speed/cooldown, per-wheel landing airtime and
separate car/car strength/cooldown contracts. Suspension calls audio before
clearing airtime; wall sparks and Crash share the original gate. Update order
and the explicit per-hit cached-speed refresh follow the recovered assembly.
Read evidence/collision_audio.md and evidence/transmission.md before editing.

Godot owns looping tire voices and separate overlapping one-shot Crash/Land
voices. `smoke_race --sfx` exercises actual vehicle triggers, then isolates native
PCM playback to check mixer signal, loop stop, simultaneous voices, one-shot
completion, reset and teardown. Its injection helpers are diagnostic fixtures,
not substitutes for recovered gameplay predicates. The stock single-car profile
does not invoke the recovered car/car sound contract until pair physics exists.
QMDX spatial attenuation and global channel stealing are not claimed reproduced.
