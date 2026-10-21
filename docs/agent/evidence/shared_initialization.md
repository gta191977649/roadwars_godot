# Shared initialization and texture selection evidence

Reference binary SHA256: `93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8`.

## Initialization scope

The retained decompilation of `0045c8ab` reads section ROADWARS from Ini/RoadWars.ini using the original config abstraction. It reads SCREENX, SCREENY, FULLSCREEN and LANGUAGE; opens Localization/Garage.bin and Localization/GarageDescriptions.bin; tests legacy DirectX/device availability; creates a Win32 window; initializes input, graphics, audio and callbacks; loads sound resources; then reaches screen/race selection through `004ab60d`.

`initialize_test_game` is an explicit host adaptation of shared configuration and data opening plus a new direct-test selection. It returns the original display/language settings, opens the garage tables as opaque bytes, and loads the requested vehicle, track and shared-track configurations. Godot owns modern window/input/render devices. Full menu initialization, legacy device/audio branches, dialogs and localization query grammar are not implemented by this helper. The original function therefore retains RE-STUB and cannot be marked fully implemented based on this path.

The host may explicitly override original display/fullscreen settings for a debugging window. The core exposes original values unchanged and has no Godot or operating-system dependency. The test profile must name one vehicle and one track; names are validated as single resource path segments. Output is committed only after every required input succeeds.

## Configuration precedence

The original config loader `004d5ad0` tries the encoded extension first, then the original plaintext path. Evidence from x86 disassembly:

```text
004d5b58 push 0061e850           ; "pse"
004d5b63 lea eax,[ebp+edx-13cf] ; last three extension characters
004d5b6b call 00516ae0          ; replace with pse
004d5b96 push 0061e854          ; "rb"
004d5ba8 call 004f7f8e          ; open encoded filename
004d5be4 call 004f80f2          ; file-open check
004d5bf0 jne 004d5caa          ; encoded file selected when open succeeded
004d5c59 push 0061e858          ; "r"
004d5c5e mov eax,[ebp+8]       ; original requested filename
004d5c68 call 004f7f8e          ; plaintext fallback only when encoded open failed
```

The actual local Ini/RoadWars.pse differs from RoadWars.ini, so this precedence has observable consequences. Encoded bytes subtract one modulo 256 before token parsing. The test fixture intentionally gives the plaintext and encoded versions different dimensions and verifies that PSE wins. A malformed existing PSE must not silently select plaintext.

The current Configuration parser is restricted to the test path and does not claim complete original token/include grammar recovery.

## Texture names, formats and black colorkey

- `004ea597` concatenates the texture directory and filename and calls the uppercase helper before examining prefixes.
- `004ecbb6` tests basename prefix `XX`, selecting a 4/4/4/4 format if available.
- `004ecc3f` tests basename prefix `16`, selecting a 5/5/5/1 format if available.
- `004ecb48` returns true exactly when the uppercase basename starts `X`. It does not inspect the first image pixel or use magenta.
- The 8-bit path `004e8dea` preserves palette-index pixels and sets a zero-valued source colorkey when `004ecb48` succeeds.
- The 8-bit-to-16-bit path `004e9389` assigns alpha zero to quantized black pixels when the X-prefix check succeeds, otherwise alpha one, and sets the zero source colorkey for that prefix.
- The 24-bit conversion path `004e9939` assigns alpha zero to quantized black pixels and sets the zero source colorkey. It does not require an X prefix in that function.
- The high-alpha path `004e9ef2` constructs a sibling Alpha.bmp name and reads the additional image. Its complete modern conversion has not been implemented by shared initialization.

These observations justify black-key handling for the sampled 8-bit X-prefixed foliage textures. They do not justify a general first-palette-color or magenta transparency rule. Retained decompilations are in shared_initialization_ghidra.json.

## Validation

The portable fixture test in tests/core/game_initialization_test.cpp was compiled directly with GNU C++20 and passed. It covers PSE precedence, plaintext fallback, opaque table loading, explicit track/vehicle selection, invalid directory segments, missing required localization, invalid display settings and transactional output on failure. No original executable was run or modified. This is infrastructure/data-path verification, not original-game behavioral equivalence.
