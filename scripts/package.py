"""Build a native Release extension and export a minimal standalone Godot package."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from local_config import ROOT, load_config
from build import build_lock

VERSION = json.loads((ROOT / 'scripts/dependencies.lock.json').read_text())['godot_version']
TARGETS = {
    'win32': ('Windows', 'windows', 'roadwars_godot.dll', 'RoadWars-RE.exe', 'windows_release_x86_64.exe'),
    'linux': ('Linux', 'linux', 'roadwars_godot.so', 'RoadWars-RE.x86_64', 'linux_release.x86_64'),
    'darwin': ('macOS', 'macos', 'roadwars_godot.dylib', 'RoadWars-RE.app', 'macos.zip'),
}

def default_templates():
    if sys.platform == 'win32':
        base = Path(os.environ.get('APPDATA', Path.home() / 'AppData/Roaming')) / 'Godot'
    elif sys.platform == 'darwin':
        base = Path.home() / 'Library/Application Support/Godot'
    else:
        base = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'godot'
    return base / 'export_templates' / (VERSION + '.stable')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--godot', type=Path, help='Matching Godot editor executable')
    parser.add_argument('--templates-dir', type=Path, default=default_templates(), help='Matching export template directory')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        raise ValueError('--jobs must be positive')
    if sys.platform not in TARGETS:
        raise ValueError('Supported native packaging hosts: Windows, Linux, macOS')
    preset, target, library, executable, template = TARGETS[sys.platform]
    template_path = args.templates_dir.resolve() / template
    version_file = template_path.parent / 'version.txt'
    if not template_path.is_file() or not version_file.is_file() or version_file.read_text().strip() != VERSION + '.stable':
        raise ValueError(f'Missing matching Godot {VERSION} export templates: {template_path}. Install templates or pass --templates-dir.')
    config = load_config() if (ROOT / 'config.local.json').is_file() else {}
    godot = str(args.godot or config.get('godot_executable') or shutil.which('godot') or shutil.which('godot4') or '')
    if sys.platform == 'win32':
        console = Path(godot).with_name(Path(godot).stem + '_console.exe')
        if console.is_file(): godot = str(console)
    if not godot:
        raise ValueError('Pass --godot or put the matching Godot editor on PATH')
    if not subprocess.check_output([godot, '--version'], text=True).startswith(VERSION + '.'):
        raise ValueError('Godot editor version does not match ' + VERSION)
    # Fixed, isolated build/staging directories; never rewrite the developer DLL.
    build = ROOT / 'build/godot-release'
    with build_lock('godot-release'):
        manifest = build / 'build.ninja'
        inputs = [ROOT / 'CMakePresets.json', ROOT / 'platform/godot/build_profile.json']
        if not (build / 'CMakeCache.txt').is_file() or not manifest.is_file() or any(p.stat().st_mtime_ns > manifest.stat().st_mtime_ns for p in inputs):
            subprocess.run(['cmake', '--preset', 'godot-release'], cwd=ROOT, check=True)
        subprocess.run(['cmake', '--build', '--preset', 'godot-release', '--parallel', str(args.jobs)], cwd=ROOT, check=True)
        extension = build / 'extension-bin' / library
        if not extension.is_file():
            raise ValueError('Native Release library was not produced: ' + str(extension))
        stage = ROOT / 'build/package-project' / target
        stage.mkdir(parents=True, exist_ok=True)
        # Allowlist rather than excluding an evolving collection of smoke scenes.
        source = ROOT / 'platform/godot/project'
        for name in ['project.godot', 'bootstrap.tscn', 'roadwars.gdextension']:
            shutil.copy2(source / name, stage / name)
        (stage / 'bin').mkdir(exist_ok=True)
        shutil.copy2(extension, stage / 'bin' / library)
        # Stage only this host's library declaration: other hosts need native builds.
        manifest = (stage / 'roadwars.gdextension').read_text().split('[libraries]')[0]
        key = {'windows': 'windows.x86_64', 'linux': 'linux.x86_64', 'macos': 'macos'}[target]
        (stage / 'roadwars.gdextension').write_text(manifest + '[libraries]\n' + key + ' = "res://bin/' + library + '"\n')
        presets = (ROOT / 'platform/godot/export_presets.cfg').read_text()
        index = ['Windows', 'Linux', 'macOS'].index(preset)
        marker = f'[preset.{index}.options]'
        escaped = str(template_path).replace('\\', '/')
        presets = presets.replace(marker, marker + '\ncustom_template/release="' + escaped + '"')
        (stage / 'export_presets.cfg').write_text(presets)
        destination = ROOT / 'build/bin' / target
        destination.mkdir(parents=True, exist_ok=True)
        subprocess.run([godot, '--headless', '--path', str(stage), '--editor', '--import', '--quit'], cwd=stage, check=True)
        subprocess.run([godot, '--headless', '--path', str(stage), '--export-release', preset, str(destination / executable)], cwd=stage, check=True)
        if not (destination / executable).exists():
            raise ValueError('Export did not produce ' + executable)
        print('Standalone Release package:', destination, flush=True)
    return 0

if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
