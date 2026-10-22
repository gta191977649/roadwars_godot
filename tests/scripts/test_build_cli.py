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
    def test_race_defaults_and_explicit_options(self):
        for arguments in (['--race'], ['run', '--race', '--track', 'dark forest',
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
                self.assertIn('-DCMAKE_BUILD_TYPE=Debug', commands[0])
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
                     ['core', '--car', 'Animal'], ['--debug']]
        for values in arguments:
            with self.subTest(arguments=values), patch.object(build, 'run') as run, \
                 patch.object(sys, 'argv', ['build.py', *values]), \
                 contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                build.main()
            run.assert_not_called()

    def test_jobs_forwarded_before_or_after_action(self):
        for arguments in (['--jobs', '8', 'core'], ['core', '--jobs', '8']):
            with self.subTest(arguments=arguments), patch.object(build, 'run') as run, \
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
                 patch.object(sys, 'argv', ['build.py', '--debug', 'trace', '--jobs', '8', 'Dark Forest']):
                build.main()
            track.assert_called_once_with('Dark Forest')
            commands = [call.args[0] for call in run.call_args_list]
            self.assertIn('-DCMAKE_BUILD_TYPE=Debug', commands[0])
            self.assertEqual(commands[1][-2:], ['--parallel', 8])
            self.assertIn('--race-track=Dark Forest', commands[-1])
            self.assertIn('--game-dir=' + str(directory), commands[-1])
            self.assertIn('--race-vehicle=Animal', commands[-1])
            self.assertIn('--debug', commands[-1])
            self.assertLess(commands[-1].index('--'), commands[-1].index('--debug'))
