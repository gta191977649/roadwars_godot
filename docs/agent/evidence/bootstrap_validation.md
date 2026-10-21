# Bootstrap validation

Date: 2026-10-22 (Australia/Sydney). These checks validate project infrastructure, not original game behavior.

## Environment and dependency identity

- Windows x64, GNU C++ 13.2.0 (MinGW), CMake 3.29.2, Ninja.
- Scripts ran with Python 3.14.4; CMake/CTest selected Python 3.10.11. Both exercised the tracking tests successfully.
- Godot `4.7.2.stable.official.ed1daf0bf`, standard single precision.
- godot-cpp commit `cedbd9e71b24eb2898b28867c5faf6114da9e5e2`, explicitly selecting included API 4.7, not its default/latest API.
- godot-cpp archive SHA256: `84f19255b8f65764d0d6de6e0b8109ddbc446cbc117d5d75e16c73dabed2250e`.
- Godot Windows archive SHA256: `731980f9608d61333e5baf54a2ef17210acc7a538446c0cb9969f002aca1e953`, checked against release metadata before extraction to ignored build/tools.

## Completed checks

| Check | Result |
| --- | --- |
| Portable core configure/build with Godot disabled | Passed |
| GDExtension configure/build with locked bindings and API 4.7 | Passed |
| Core CTest suite | 4/4 passed: core contract, Python tracking suite, dependency boundary, progress consistency |
| Godot-build CTest suite | 4/4 passed, same four checks |
| Python tracking tests | 7 tests passed, including merged mappings, invalid ownership/status, missing verification evidence, duplicate IDs, stale mappings/docs, idempotent merge, binary mismatch and incomplete export rejection |
| Actual Godot extension load and headless lifecycle | Passed |
| Compatibility renderer image acceptance | Passed: 33,282 green triangle pixels |
| Ten renderer/runtime restarts | Passed: six owned RIDs during each smoke scene, zero after tree exit |
| Engine shutdown diagnostics | No errors or leak warnings reported in successful smoke execution |
| Visual inspection of build/godot_smoke.png | Triangle centered on dark viewport; no original assets needed |

Commands: `python scripts/build.py core`, `python scripts/build.py godot`, `python scripts/build.py test --preset core`, `python scripts/build.py test --preset godot`, `python scripts/smoke_godot.py`.

## Inventory coverage

All function-list and RTTI pagination cursors were exhausted. Ghidra MCP yielded 8,603 internal functions and 121 RTTI types. The hash-matched PE import table added 692 IAT entries, yielding the metadata total of 9,295 functions. The CSV has 9,416 rows including types. External function rows use IAT virtual addresses, not function bodies or synthetic Ghidra EXTERNAL addresses.

Named five-byte E9 jump thunks are classified using bytes from the same hash-matched binary. Other unknown functions/types remain unresolved; no gameplay class or original source directory is invented. None of the original game functions are marked implemented or verified by this bootstrap.

## Limits

Linux/macOS builds and graphics backends other than Windows Compatibility were not tested. The future Ghidra Java refresh script was provided but not executed inside Ghidra during bootstrap; the initial inventory was collected through the connected MCP and the PE supplement was executed. Any refresh must compare coverage before accepting the export. RID counts are adapter ownership checks corroborated by clean engine diagnostics, not a formal proof of absence of every engine-internal allocation leak.

The original game was not executed, original gameplay was not implemented, and no behavioral equivalence claim is made. No Git repository or commit was created. No sibling project code or findings were imported.
