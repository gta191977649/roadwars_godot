# Standalone distribution and asset-root startup

This is modern host infrastructure, not recovered original behavior. It has no
RE function mappings and does not advance reverse-engineering completion.

## Build and distribution

`python scripts/package.py --templates-dir <matching-template-directory>` builds
the native `godot-release` preset, imports an allowlisted staging project and
exports through the matching Godot editor. `--godot` overrides the tool location;
`--jobs` controls compilation. The editor version and template `version.txt` must
match the dependency lock. Missing templates are an explicit error; they are not
automatically downloaded. The default template location follows Godot's native
Windows/Linux/macOS data-directory convention.

Windows output is `build/bin/windows/RoadWars-RE.exe` and `roadwars_godot.dll`.
The project PCK is embedded in the EXE. Keep these two files together; no editor,
source checkout, test scenes, developer config or original assets are distributed.
The extension output is isolated in `build/godot-release/extension-bin`; Debug
continues to use the development project's `bin`. Linux x86_64 and macOS export
presets use their native shared libraries and need native builds. macOS uses the
executable within the application bundle when walking ancestors.

## Runtime startup

An explicit `--game-dir` user argument has highest priority. Otherwise explicit
`--debug` enables the existing repository-local developer config or an optional
executable-adjacent `config.local.json`. Ordinary startup never consults that
config. Missing/invalid developer configuration falls through to detection.
Detection walks the process working directory and then executable directory,
nearest ancestor first, up to the filesystem root; it never scans child trees.

Godot's official Release templates change process CWD to the executable directory
(or macOS bundle resources) before loading the extension. Consequently a caller's
shell working directory cannot be recovered by the extension with those templates.
Executable/ancestor discovery still works; use explicit `--game-dir` for a remote
installation. Custom Godot templates built with `disable_path_overrides=no` retain
the caller's CWD. This engine behavior is visible in Godot 4.7.2 `main/main.cpp`,
`Main::setup`, lines 1044-1057. No OS-specific process-inspection workaround or
modified original binary is used.

Validation checks readable Data.pkh/Data.pak and Cars/Tracks/Sfx directories, then
uses the existing portable ResourceStore to check the initial configuration,
main-menu configuration and packed frontend fonts. Encoded .pse configuration is
accepted through the same resource policy. This is an early root check; normal
loaders retain content validation and report malformed/missing later resources.
Case-insensitive component matching allows original Windows path spellings on
case-sensitive hosts. Ambiguous variants are rejected. Successfully resolved
loose-file paths are cached per GameFiles owner, not written to disk.

When no root is available, a host-only prompt offers Browse and Exit. Godot
FileDialog uses filesystem directory mode and native dialogs where supported,
with Godot's fallback elsewhere. Cancel returns to the prompt; invalid selections
show missing/unreadable-resource details and allow retry. The chosen root is held
only in memory. No installation/user path setting is saved. A headless failure
reports the issue and exits nonzero instead of waiting for unavailable UI.

No menu, race or audio owner starts while waiting. Host input and simulation are
idle; initialization starts a fresh monotonic clock. Editor scene import never
runs the game bootstrap. Startup diagnostics expose waiting/root/error/initialized
state and validation results without adding a gameplay interface.

The window title updates twice per second using the actual RenderingServer driver
and engine FPS: `RoadWars-RE (Vulkan) FPS: 120 - Release | By Nurupo`. Backend names
include OpenGL, Vulkan, Metal and Direct3D 12 as applicable. Runtime debug mode,
rather than compiler optimization level, determines Debug/Release in this title.

## Validation and platform limits

Core CTest passed 67/67; native adapter CTest passed 2/2. Debug and Release
extension builds and the Windows embedded-pack export passed. The startup smoke
covers relative/absolute explicit paths, debug configuration, normal-mode config
isolation, case-aware Unicode fixtures, retry/cancel and in-memory-only selection.
Its native host checks exercise callbacks and title format; they do not establish
manual native-dialog interaction. The user requested to perform manual UI testing,
so no computer-use input was performed against the game.

The exported EXE smoke runs independently from the repository with only EXE+DLL:
root/bin/deep-ancestor installation placement, isolated missing-assets failure,
explicit remote root, and a real Single Race load. Frontend race/return/BGM smoke,
developer-config menu smoke, competition lifecycle smoke and diagnostic renderer
lifecycle smoke passed. Final direct-debug acceptance is recorded after execution.

Linux/macOS runtime and dialog behavior have not been tested on this Windows host.
The existing frontend GSM 6.10 audio adapter remains Windows-specific; this work
does not claim complete game execution on other hosts. Native directory picking,
manual double-click interaction and visual title review remain user acceptance.
