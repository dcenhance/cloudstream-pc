import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from release_identity import release_version, identity


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(filename))
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class ReleasePackagingTests(unittest.TestCase):
    def test_installer_writes_installed_identity_without_mutating_portable(self):
        installer = load('installer_under_test', 'build-windows-installer.py')
        with tempfile.TemporaryDirectory() as folder:
            runtime = Path(folder) / 'runtime'
            for name in ('cloudstream.exe', 'runtime/bin/java.exe', 'platforms/qwindows.dll'):
                path = runtime / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'TEST FIXTURE, NOT AN EXECUTABLE')
            marker = runtime / 'cloudstream-build.json'
            marker.write_text(identity('windows-zip'))
            (runtime / 'cloudstream-version.txt').write_text(release_version())
            scripts = []
            def capture(args, **kwargs):
                scripts.append(Path(args[-1]).read_text())
            with patch('sys.argv', ['installer', str(runtime), str(Path(folder) / 'Setup.exe')]), patch.object(installer.subprocess, 'run', capture):
                installer.main()
            self.assertIn('windows-setup', scripts[0])
            self.assertIn('cloudstream-build.json', scripts[0])
            self.assertEqual(json.loads(marker.read_text())['package'], 'windows-zip')

    def test_version_is_single_source_and_invalid_package_is_rejected(self):
        self.assertEqual(json.loads(identity('appimage'))['version'], release_version())
        with self.assertRaises(ValueError):
            identity('appimage', '9.9.9')
        with self.assertRaises(ValueError):
            identity('arbitrary-executable')


if __name__ == '__main__':
    unittest.main()
