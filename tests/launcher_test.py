"""Exercise the generated Desktop Entry through GIO, without a user-bin PATH."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]

@unittest.skipUnless(shutil.which('gio'), 'GIO is not installed')
class LauncherTest(unittest.TestCase):
    def test_prefixes_resolve_without_source_folder_or_user_bin_path(self):
        for name in ['Normal', 'Space path', 'Install $literal % and "quotes"']:
            with self.subTest(prefix=name), tempfile.TemporaryDirectory(prefix='ao3-launch-test-') as temporary:
                root = Path(temporary)
                prefix = root / name
                (prefix / 'bin').mkdir(parents=True)
                (prefix / 'share/applications').mkdir(parents=True)
                desktop = prefix / 'share/applications/ao3-client.desktop'
                desktop.write_text((ROOT / 'ao3-client.desktop').read_text())
                executable = prefix / 'bin/ao3-client'
                executable.write_text('#!/bin/sh\nprintf verified > "$AO3_LAUNCH_MARKER"\n')
                executable.chmod(0o755)
                subprocess.run([sys.executable, str(ROOT / 'scripts/write-launcher.py'), str(prefix)], check=True)
                env = os.environ.copy()
                env.update(PATH='/usr/bin:/bin', XDG_CONFIG_HOME=str(root / 'config'), AO3_LAUNCH_MARKER=str(root / 'launched'))
                result = subprocess.run([shutil.which('gio'), 'launch', str(desktop)], cwd=temporary, env=env, capture_output=True, text=True, timeout=5)
                self.assertEqual(result.returncode, 0, result.stderr)
                deadline = time.monotonic() + 3
                while not (root / 'launched').exists() and time.monotonic() < deadline:
                    time.sleep(.02)
                self.assertEqual((root / 'launched').read_text(), 'verified')
                self.assertNotIn(str(ROOT), desktop.read_text())

if __name__ == '__main__':
    unittest.main()
