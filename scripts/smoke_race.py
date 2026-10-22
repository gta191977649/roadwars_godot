"""Run original-asset scene and driving acceptance using the unified local configuration."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
from local_config import ROOT, game_directory, load_config, resolve_track


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--frontend', action='store_true', help='Check native HUD textures and player map motion')
    parser.add_argument('--debug', action='store_true', help='Enable runtime Dev Build atlas label')
    parser.add_argument('--preview', action='store_true', help='Only validate asset rendering and lifecycle')
    parser.add_argument('--core', action='store_true', help='Run the 20-second portable Dark Forest driving probe without Godot')
    parser.add_argument('--frame-independent', action='store_true', help='Compare complete 20-second physics traces across render rates and stalls')
    parser.add_argument('--timing', action='store_true', help='Compare Godot host physics/effects and interpolated presentation across render rates')
    parser.add_argument('--particles', action='store_true', help='Check original collision-triggered native Godot particles')
    parser.add_argument('--camera', action='store_true', help='Check recovered camera and explicit mouse orbit')
    parser.add_argument('--music', action='store_true', help='Check original race BGM playback, loop and lifecycle')
    parser.add_argument('--audio', action='store_true', help='Check original PCM playback through the native audio mixer')
    parser.add_argument('--sfx', action='store_true', help='Check actual skid/collision triggers and native SFX voices')
    parser.add_argument('--track', default='Dark Forest')
    args = parser.parse_args()
    track = resolve_track(args.track)
    if args.core or args.frame_independent:
        if track != 'Dark Forest' or args.preview:
            raise ValueError('The portable driving probe currently requires Dark Forest and no --preview')
        name = 'roadwars_frame_independence_probe' if args.frame_independent else 'roadwars_vehicle_probe'
        probe = ROOT / 'build/core' / (name + ('.exe' if os.name == 'nt' else ''))
        if not probe.is_file():
            raise ValueError('Build the portable core before running --core')
        result = subprocess.run([str(probe), str(game_directory())], cwd=ROOT, capture_output=True, text=True, timeout=60)
        print(result.stdout, result.stderr)
        marker = 'no elapsed time dropped' if args.frame_independent else 'Actual wall contacts='
        if result.returncode or marker not in result.stdout:
            return 1
        return 0
    executable = Path(load_config()['godot_executable'])
    if os.name == 'nt':
        console = executable.with_name(executable.stem + '_console.exe')
        if console.is_file():
            executable = console
    version = subprocess.check_output([str(executable), '--version'], text=True).strip()
    if not version.startswith('4.7.2.'):
        raise ValueError('Godot 4.7.2 required, found ' + version)
    script = 'music_smoke_test.gd' if args.music else 'frontend_smoke_test.gd' if args.frontend else 'timing_smoke_test.gd' if args.timing else 'sfx_smoke_test.gd' if args.sfx else 'audio_smoke_test.gd' if args.audio else 'camera_smoke_test.gd' if args.camera else 'particle_smoke_test.gd' if args.particles else ('race_smoke_test.gd' if args.preview else 'drive_smoke_test.gd')
    capture = ROOT / 'build' / ('race_frontend.png' if args.frontend else 'race_camera.png' if args.camera else 'race_particles.png' if args.particles else ('race_preview.png' if args.preview else 'race_drive.png'))
    if not args.headless and not (args.music or args.audio or args.sfx or args.timing):
        capture.unlink(missing_ok=True)
    command = [str(executable), '--path', str(ROOT / 'platform/godot/project'), '--script', 'res://' + script]
    if args.headless:
        command.append('--headless')
    command += ['--', '--no-debug-overlay', '--race-track=' + track, '--race-vehicle=Animal',
                '--game-dir=' + str(game_directory()), '--capture=' + str(capture)]
    if args.preview:
        command.append('--race-preview')
    if args.debug:
        command.append('--debug')
    log_path = ROOT / 'build' / (script + '.log')
    log_path.unlink(missing_ok=True)
    command[1:1] = ['--log-file', str(log_path)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=60)
    log = log_path.read_text(encoding='utf-8', errors='replace') if log_path.is_file() else ''
    output = result.stdout + '\n' + result.stderr + '\n' + log
    print(output)
    if not args.headless and not any('Vulkan ' in line and ' - Forward+ - ' in line
                                     for line in output.splitlines()):
        print('Expected the default Vulkan Forward+ renderer', file=sys.stderr)
        return 1
    marker = 'ROADWARS_MUSIC_SMOKE_OK' if args.music else 'ROADWARS_FRONTEND_SMOKE_OK' if args.frontend else 'ROADWARS_TIMING_SMOKE_OK' if args.timing else 'ROADWARS_SFX_SMOKE_OK' if args.sfx else 'ROADWARS_AUDIO_SMOKE_OK' if args.audio else 'ROADWARS_CAMERA_SMOKE_OK' if args.camera else 'ROADWARS_PARTICLE_SMOKE_OK' if args.particles else ('ROADWARS_RACE_SMOKE_OK' if args.preview else 'ROADWARS_DRIVE_SMOKE_OK')
    if result.returncode or 'ERROR:' in output or 'leaked' in output.lower() or marker not in output:
        return 1
    if not args.headless and not args.music and not args.audio and not args.sfx and not args.timing:
        if not capture.is_file():
            raise ValueError('Render capture was not produced')
        print('Render capture:', capture)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
