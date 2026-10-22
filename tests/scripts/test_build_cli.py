"""The Windows trace command forwards a quoted track and the single configured root."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[2] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
spec = importlib.util.spec_from_file_location('roadwars_build', SCRIPTS / 'build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)


class BuildCommandTests(unittest.TestCase):
    def test_empty_dependency_record_rebuilds_only_its_object(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / 'build/core/target.cpp.obj'
            target.parent.mkdir(parents=True)
            target.write_bytes(b'stale')
            def compile_again(command):
                self.assertFalse(target.exists())
                target.write_bytes(b'rebuilt')
            with patch.object(build, 'ROOT', root), patch.object(build, 'configure_build'), \
                 patch.object(build, 'empty_dependency_objects', side_effect=[[target], []]), \
                 patch.object(build, 'run', side_effect=compile_again):
                build.build_target('core', 2)
            self.assertEqual(target.read_bytes(), b'rebuilt')

    def test_dependency_cache_cannot_delete_an_outside_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / 'build/core'
            directory.mkdir(parents=True)
            (directory / 'build.ninja').touch()
            with patch.object(build, 'ROOT', root), \
                 patch.object(build.subprocess, 'check_output', return_value='../outside.obj: #deps 0, deps mtime 1 (VALID)\n'), \
                 self.assertRaisesRegex(ValueError, 'Unsafe object path'):
                build.empty_dependency_objects('core')

    def test_build_only_by_default_even_for_legacy_launch_selectors(self):
        for arguments in ([], ['--debug'], ['--race'], ['run'], ['trace', 'Dark Forest']):
            with self.subTest(arguments=arguments), patch.object(build, 'build_target') as target, \
                 patch.object(build, 'load_config') as config, patch.object(build, 'run') as run, \
                 patch.object(sys, 'argv', ['build.py', *arguments]):
                build.main()
            target.assert_called_once_with('godot', 4, False)
            config.assert_not_called()
            run.assert_not_called()

    def test_incremental_cache_and_changed_profile(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            marker = root / 'platform/godot/third_party/godot-cpp/.roadwars_commit'
            marker.parent.mkdir(parents=True)
            marker.touch()
            profile = root / 'platform/godot/build_profile.json'
            profile.write_text('{}')
            (root / 'CMakePresets.json').write_text('{}')
            directory = root / 'build/godot'
            directory.mkdir(parents=True)
            (directory / 'CMakeCache.txt').write_text(
                f'CMAKE_HOME_DIRECTORY:INTERNAL={root.as_posix()}\n'
                'CMAKE_GENERATOR:INTERNAL=Ninja\nCMAKE_BUILD_TYPE:STRING=Debug\n'
                'ROADWARS_BUILD_GODOT:BOOL=ON\n')
            (directory / 'build.ninja').touch()
            with patch.object(build, 'ROOT', root), patch.object(build, 'run') as run:
                build.build_target('godot', 8)
                build.build_target('godot', 8)
                self.assertEqual(run.call_count, 2)
                self.assertTrue(all(call.args[0][:2] == ['cmake', '--build']
                                    for call in run.call_args_list))
                profile.write_text('{"enabled_classes":[]}')
                build.build_target('godot', 8)
            self.assertEqual(run.call_args_list[-2].args[0], ['cmake', '--preset', 'godot'])

    def test_clean_requests_native_clean_first_without_deleting_cache(self):
        with patch.object(build, 'configure_build') as configure, patch.object(build, 'run') as run, \
             patch.object(build, 'empty_dependency_objects', return_value=[]):
            build.build_target('core', 2, clean=True)
        configure.assert_called_once_with('core')
        self.assertEqual(run.call_args.args[0],
                         ['cmake', '--build', '--preset', 'core', '--parallel', 2, '--clean-first'])

    def test_race_defaults_and_explicit_options(self):
        for arguments in (['--run', '--race'], ['--run', 'run', '--race', '--track', 'dark forest',
                                      '--laps', '1', '--car=avenging angel', '--ai', '0', '--debug']):
            with self.subTest(arguments=arguments), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                marker = root / 'platform/godot/third_party/godot-cpp/.roadwars_commit'
                marker.parent.mkdir(parents=True)
                marker.touch()
                config = {'godot_executable': str(root / 'godot.exe')}
                (root / 'config.local.json').write_text(json.dumps(config))
                with patch.object(build, 'ROOT', root), \
                     patch.object(build, 'resolve_track', return_value='Dark Forest'), \
                     patch.object(build, 'resolve_car', return_value='Avenging Angel') as car, \
                     patch.object(build, 'game_directory', return_value=root / 'game'), \
                     patch.object(build, 'load_config', return_value=config), \
                     patch.object(build, 'run') as run, \
                     patch.object(build.subprocess, 'check_output', return_value='4.7.2.stable.test'), \
                     patch.object(sys, 'argv', ['build.py', *arguments]):
                    build.main()
                commands = [call.args[0] for call in run.call_args_list]
                self.assertEqual(commands[0], ['cmake', '--preset', 'godot'])
                launch = commands[-1]
                self.assertIn('--race', launch)
                explicit = '--debug' in arguments
                self.assertIn('--race-vehicle=' + ('Avenging Angel' if explicit else 'Animal'), launch)
                self.assertIn('--race-laps=' + ('1' if explicit else '3'), launch)
                self.assertIn('--race-ai=' + ('0' if explicit else '5'), launch)
                self.assertEqual('--debug' in launch, explicit)
                self.assertEqual(car.call_count, 1 if explicit else 0)

    def test_invalid_race_options_fail_before_running_commands(self):
        import contextlib
        import io
        arguments = [['--race', '--ai', '-1'], ['--race', '--ai', '6'],
                     ['--race', '--laps', '0'], ['--race', '--laps', '1.5'],
                     ['--race', '--laps', '11'], ['--race', '--diagnostic'],
                     ['--race', '--original-loop'], ['--race', '--frontend-screen', 'title'],
                     ['trace', 'Dark Forest', '--race'], ['run', '--laps', '3'],
                     ['core', '--car', 'Animal'], ['core', '--run'], ['test', '--clean'], ['trace']]
        for values in arguments:
            with self.subTest(arguments=values), patch.object(build, 'run') as run, \
                 patch.object(sys, 'argv', ['build.py', *values]), \
                 contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build.main()
            run.assert_not_called()

    def test_jobs_forwarded_before_or_after_action(self):
        for arguments in (['--jobs', '8', 'core'], ['core', '--jobs', '8']):
            with self.subTest(arguments=arguments), patch.object(build, 'run') as run, \
                 patch.object(build, 'empty_dependency_objects', return_value=[]), \
                 patch.object(sys, 'argv', ['build.py', *arguments]):
                build.main()
                self.assertEqual(run.call_args.args[0],
                                 ['cmake', '--build', '--preset', 'core', '--parallel', 8])

    def test_invalid_jobs_fail_before_running_commands(self):
        import contextlib
        import io
        for value in ('0', '-1', 'many'):
            with self.subTest(value=value), patch.object(build, 'run') as run, \
                 patch.object(sys, 'argv', ['build.py', 'core', '--jobs', value]), \
                 contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as error:
                build.main()
            self.assertEqual(error.exception.code, 2)
            run.assert_not_called()

    def test_debug_trace_forwards_track_and_game_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            marker = root / 'platform/godot/third_party/godot-cpp/.roadwars_commit'
            marker.parent.mkdir(parents=True)
            marker.touch()
            config = {'godot_executable': str(root / 'godot.exe')}
            (root / 'config.local.json').write_text(json.dumps(config))
            directory = root / 'original game'
            with patch.object(build, 'ROOT', root), \
                 patch.object(build, 'resolve_track', return_value='Dark Forest') as track, \
                 patch.object(build, 'game_directory', return_value=directory), \
                 patch.object(build, 'load_config', return_value=config), \
                 patch.object(build, 'run') as run, \
                 patch.object(build.subprocess, 'check_output', return_value='4.7.2.stable.test'), \
                 patch.object(sys, 'argv', ['build.py', '--run', '--debug', 'trace', '--jobs', '8', 'Dark Forest']):
                build.main()
            track.assert_called_once_with('Dark Forest')
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(commands[0], ['cmake', '--preset', 'godot'])
            self.assertEqual(commands[1][-2:], ['--parallel', 8])
            self.assertIn('--race-track=Dark Forest', commands[-1])
            self.assertIn('--game-dir=' + str(directory), commands[-1])
            self.assertIn('--race-vehicle=Animal', commands[-1])
            self.assertIn('--debug', commands[-1])
            self.assertLess(commands[-1].index('--'), commands[-1].index('--debug'))
