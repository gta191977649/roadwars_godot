# RoadWars — C++ / Godot reconstruction

An independent reconstruction of RoadWars, with portable C++20 game logic and
Godot rendering, input and audio through GDExtension.

**Work in progress:** menus, driving, AI opponents and vehicle combat are partially
reconstructed. This is not yet a complete, fully verified replacement for the
original game. Live gameplay uses a fixed 60 Hz simulation with interpolated rendering.

## Build and run

Windows x64 is the currently validated platform. You need Python 3.10+, CMake 3.24+,
Ninja, a C++20 compiler and Godot **4.7.2** (standard single precision).

1. Copy `config.example.json` to `config.local.json`.
2. Set `godot_executable` to your Godot executable and `game_dir` to your original
   game installation containing `Data.pak` and `Data.pkh`.
3. From the repository root, run:

```powershell
python scripts/build.py deps
python scripts/build.py --run
```

Normal launch opens the frontend and detects or asks for the original installation.
To try a race using your configured installation:

```powershell
.\build.ps1 --run --race --track "Dark Forest" --car animal --ai 5 --debug
```

Use arrow keys and Enter in menus; Escape goes back or pauses a race.
Run with `--diagnostic` for a simple rendering test without original assets.

## Build and test the portable core

No Godot installation or original assets are needed:

```powershell
python scripts/build.py core
python scripts/build.py test --preset core
```

## Source layout

- `game_src/`: portable game logic and rendering contracts.
- `platform/godot/`: Godot project and engine adapters.
- `scripts/`: builds, packaging and validation tools.
- `tests/`: core and tooling tests.

Original game files are not included; use your own installation. Local AI guides,
research notes and generated function inventories stay out of version control.
Research inventory checks run only when their local records are available.
