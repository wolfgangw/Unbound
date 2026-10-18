#!/usr/bin/env python3
"""Exercise the real shell installer in isolated user roots with mocked OS checks."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[5]
INSTALL = ROOT / 'infrastructure/macos/lua/install.sh'
STARTUP = ROOT / 'infrastructure/macos/lua/startup'
UNINSTALL = ROOT / 'infrastructure/macos/lua/uninstall.sh'
if '--scripts-dir' in sys.argv:
    index = sys.argv.index('--scripts-dir')
    scripts_dir = Path(sys.argv[index + 1]).resolve()
    INSTALL, UNINSTALL, STARTUP = scripts_dir / 'install.sh', scripts_dir / 'uninstall.sh', scripts_dir / 'startup'
    del sys.argv[index:index + 2]


class Installation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='unbound install ')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.user = self.base / 'User With Spaces'
        self.support = self.user / 'Library/Application Support/Unbound'
        self.scripts = self.user / 'Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts'
        self.runtime = self.support / 'Runtime'
        self.hook = self.scripts / 'Unbound.scriptlib'
        self.candidate = self.base / 'candidate'
        self.write(self.candidate / 'VERSION', '0.3.0\n')
        self.write(self.candidate / 'BUILD', '2026.10.18.010\n')
        self.write(self.candidate / 'runtime/load.lua', 'private loader')
        for name in ('UnboundLuaLoader.dylib', 'UnboundHelper.dylib'):
            self.write(self.candidate / 'runtime' / name, 'signed fixture')
        self.write(self.candidate / 'startup/Unbound.scriptlib', 'new hook')
        self.bin = self.base / 'bin'
        self.bin.mkdir()
        self.mock('pgrep', 'import os, sys\nsys.exit(0 if os.getenv("FIXTURE_RESOLVE_RUNNING") else 1)')
        self.mock('codesign', '''import os, sys
from pathlib import Path
sys.exit(1 if os.getenv('FIXTURE_BAD_SIGNATURE') or not Path(sys.argv[-1]).is_file() else 0)''')
        self.mock('mv', '''import os, sys, subprocess
if os.getenv('FIXTURE_FAIL_UNINSTALL') and sys.argv[-2].endswith('/Runtime'):
    sys.exit(1)
if (os.getenv('FIXTURE_FAIL_PUBLISH') and sys.argv[-1].endswith('/Scripts/Unbound.scriptlib')
        and Path(sys.argv[-2]).name == 'Unbound.scriptlib'):
    sys.exit(1)
sys.exit(subprocess.call(['/bin/mv'] + sys.argv[1:]))'''.replace('import os, sys, subprocess', 'import os, sys, subprocess\nfrom pathlib import Path'))
        self.env = dict(os.environ, UNBOUND_USER_HOME=str(self.user), PATH=str(self.bin) + os.pathsep + os.environ['PATH'])
        # Sentinels must survive every install/migration/rollback.
        self.settings = self.support / 'Unbound.json'
        self.other = self.scripts / 'Utility/Other.lua'
        for path in (self.settings, self.other):
            self.write(path, 'untouched')

    @staticmethod
    def write(path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def mock(self, name, body):
        path = self.bin / name
        path.write_text('#!/usr/bin/env python3\n' + body + '\n')
        path.chmod(0o755)

    def run_script(self, script=None, *, success=True, extra_args=(), **env):
        script = script or INSTALL
        args = ['/bin/bash', str(script)]
        if script.name in ('install.sh', 'enable.sh'):
            args.append(str(self.candidate))
        args.extend(extra_args)
        result = subprocess.run(args, env=dict(self.env, **env), text=True, capture_output=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        for path in (self.settings, self.other):
            self.assertEqual(path.read_text(), 'untouched')
        return result

    def seed_previous(self):
        self.write(self.runtime / 'load.lua', 'previous private loader')
        self.write(self.hook, 'previous hook')
        self.write(self.scripts / 'Utility/Unbound.lua', 'previous menu launcher')
        self.write(self.scripts / 'Utility/UnboundLua/UnboundHelper.dylib', 'previous helper')

    def assert_installed(self):
        self.assertEqual(self.runtime.joinpath('VERSION').read_text(), '0.3.0\n')
        self.assertEqual(self.runtime.joinpath('BUILD').read_text(), '2026.10.18.010\n')
        self.assertEqual(self.hook.read_text(), 'new hook')
        self.assertEqual(self.runtime.joinpath('load.lua').read_text(), 'private loader')
        self.assertFalse(self.scripts.joinpath('Utility/Unbound.lua').exists())
        self.assertFalse(self.scripts.joinpath('Utility/UnboundLua').exists())
        self.assertEqual(sorted(p.relative_to(self.scripts).as_posix() for p in self.scripts.rglob('*') if p.is_file()),
                         ['Unbound.scriptlib', 'Utility/Other.lua'])

    def test_fresh_install(self):
        self.run_script()
        self.assert_installed()

    def test_migrate_old_menu_installation(self):
        self.seed_previous()
        self.run_script()
        self.assert_installed()
        backups = list(self.support.joinpath('InstallationBackups').glob('install.*'))
        self.assertEqual(len(backups), 1)
        previous = backups[0]
        self.assertEqual(previous.joinpath('previous-runtime/load.lua').read_text(), 'previous private loader')
        self.assertEqual(previous.joinpath('previous-Unbound.lua').read_text(), 'previous menu launcher')
        self.assertEqual(previous.joinpath('previous-UnboundLua/UnboundHelper.dylib').read_text(), 'previous helper')
        self.assertEqual(previous.joinpath('previous-Unbound.scriptlib').read_text(), 'previous hook')

    def test_update_disable_and_reenable(self):
        self.run_script()
        self.write(self.candidate / 'runtime/load.lua', 'updated loader')
        self.run_script()
        self.assertEqual(self.runtime.joinpath('load.lua').read_text(), 'updated loader')
        self.run_script(STARTUP / 'disable.sh')
        self.assertFalse(self.hook.exists())
        self.assertEqual(self.runtime.joinpath('load.lua').read_text(), 'updated loader')
        self.run_script(STARTUP / 'enable.sh')
        self.assertEqual(self.hook.read_text(), 'new hook')
        self.assertFalse(self.scripts.joinpath('Utility/Unbound.lua').exists())

    def test_resolve_running_refuses_before_changes(self):
        self.seed_previous()
        self.run_script(success=False, FIXTURE_RESOLVE_RUNNING='1')
        self.assert_previous()
        self.assertFalse(self.support.joinpath('InstallationBackups').exists())

    def test_invalid_signature_refuses_before_changes(self):
        self.seed_previous()
        self.run_script(success=False, FIXTURE_BAD_SIGNATURE='1')
        self.assert_previous()

    def test_missing_helper_refuses_before_changes(self):
        self.seed_previous()
        self.candidate.joinpath('runtime/UnboundHelper.dylib').unlink()
        self.run_script(success=False)
        self.assert_previous()

    def test_publish_failure_rolls_back(self):
        self.seed_previous()
        self.run_script(success=False, FIXTURE_FAIL_PUBLISH='1')
        self.assert_previous()

    def test_first_install_failure_leaves_no_activation(self):
        self.run_script(success=False, FIXTURE_FAIL_PUBLISH='1')
        self.assertFalse(self.hook.exists())
        self.assertFalse(self.runtime.exists())

    def test_uninstall_preserves_settings_and_other_files(self):
        self.seed_previous()
        self.write(self.support / 'StartupBackups/old.scriptlib', 'disabled hook')
        self.run_script()
        self.run_script(UNINSTALL)
        self.assertFalse(self.runtime.exists())
        self.assertFalse(self.hook.exists())
        self.assertFalse(self.scripts.joinpath('Utility/Unbound.lua').exists())
        self.assertFalse(self.scripts.joinpath('Utility/UnboundLua').exists())
        self.assertFalse(self.support.joinpath('InstallationBackups').exists())
        self.assertFalse(self.support.joinpath('StartupBackups').exists())
        self.run_script()  # A reinstall keeps saved settings available.
        self.assert_installed()

    def test_uninstall_rejects_obsolete_settings_removal_option(self):
        self.run_script()
        self.write(self.support / 'settings.json', 'legacy settings')
        self.write(self.support / 'unrelated.txt', 'keep')
        self.run_script(UNINSTALL, success=False, extra_args=('--remove-settings',))
        self.assertEqual(self.support.joinpath('settings.json').read_text(), 'legacy settings')
        self.assertEqual(self.support.joinpath('unrelated.txt').read_text(), 'keep')
        self.assert_installed()

    def test_repeated_uninstall_preserves_current_and_legacy_settings(self):
        self.run_script()
        self.write(self.support / 'settings.json', 'legacy settings')
        self.run_script(UNINSTALL)
        self.assertFalse(self.runtime.exists())
        self.assertEqual(self.settings.read_text(), 'untouched')
        self.run_script(UNINSTALL)
        self.assertEqual(self.settings.read_text(), 'untouched')
        self.assertEqual(self.support.joinpath('settings.json').read_text(), 'legacy settings')
        self.assertFalse(self.runtime.exists())
        self.assertFalse(self.hook.exists())

    def test_uninstall_refuses_running_resolve(self):
        self.seed_previous()
        self.run_script(UNINSTALL, success=False, FIXTURE_RESOLVE_RUNNING='1')
        self.assert_previous()

    def test_uninstall_rollback_restores_startup(self):
        self.seed_previous()
        self.run_script(UNINSTALL, success=False, FIXTURE_FAIL_UNINSTALL='1')
        self.assert_previous()

    def test_uninstall_without_installation_is_harmless(self):
        self.run_script(UNINSTALL)
        self.run_script(UNINSTALL)
        self.assertFalse(self.hook.exists())

    def test_uninstall_does_not_follow_runtime_symlink(self):
        outside = self.base / 'Unrelated Runtime'
        self.write(outside / 'keep.txt', 'keep')
        self.runtime.symlink_to(outside, target_is_directory=True)
        self.run_script(UNINSTALL)
        self.assertFalse(self.runtime.is_symlink())
        self.assertEqual(outside.joinpath('keep.txt').read_text(), 'keep')

    def assert_previous(self):
        self.assertFalse(self.runtime.joinpath('VERSION').exists())
        self.assertFalse(self.runtime.joinpath('BUILD').exists())
        self.assertEqual(self.runtime.joinpath('load.lua').read_text(), 'previous private loader')
        self.assertEqual(self.hook.read_text(), 'previous hook')
        self.assertEqual(self.scripts.joinpath('Utility/Unbound.lua').read_text(), 'previous menu launcher')
        self.assertEqual(self.scripts.joinpath('Utility/UnboundLua/UnboundHelper.dylib').read_text(), 'previous helper')


if __name__ == '__main__':
    unittest.main()
