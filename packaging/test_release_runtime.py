import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from release_runtime import refresh_provider_host


class ReleaseRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.runtime = self.root / 'runtime'
        self.host = self.runtime / 'provider-host/lib'
        self.host.mkdir(parents=True)
        self.licenses = self.runtime / 'licenses'
        self.licenses.mkdir()
        self.distribution = self.root / 'built/lib'
        self.distribution.mkdir(parents=True)
        self.old = b'old provider code'
        self.new = b'new provider code'
        self.dependency = b'unchanged third-party dependency'
        (self.host / 'provider-host-4.8.0.jar').write_bytes(self.old)
        (self.host / 'dependency.jar').write_bytes(self.dependency)
        (self.distribution / 'provider-host-4.8.0.jar').write_bytes(self.new)
        self.audit = self.root / 'jvm-provenance.json'
        self.audit.write_text(json.dumps([
            {'jar': 'provider-host-4.8.0.jar', 'sha256': hashlib.sha256(self.new).hexdigest(), 'status': 'LOCAL'},
            {'jar': 'dependency.jar', 'sha256': hashlib.sha256(self.dependency).hexdigest(), 'coordinate': 'example:dependency:1'},
        ]))

    def test_refreshes_only_first_party_jar_and_license_manifest(self):
        changed = refresh_provider_host(self.runtime, self.distribution.parent, self.audit)
        self.assertEqual(changed, ['provider-host/lib/provider-host-4.8.0.jar', 'licenses/jvm-provenance.json'])
        self.assertEqual((self.host / 'provider-host-4.8.0.jar').read_bytes(), self.new)
        self.assertEqual((self.host / 'dependency.jar').read_bytes(), self.dependency)
        manifest = json.loads((self.licenses / 'jvm-provenance.json').read_text())
        self.assertEqual(manifest[0]['sha256'], hashlib.sha256(self.new).hexdigest())
        self.assertEqual(manifest[1]['sha256'], hashlib.sha256(self.dependency).hexdigest())

    def test_rejects_changed_dependency_before_mutating_runtime(self):
        (self.host / 'dependency.jar').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'dependency.jar'):
            refresh_provider_host(self.runtime, self.distribution.parent, self.audit)
        self.assertEqual((self.host / 'provider-host-4.8.0.jar').read_bytes(), self.old)
        self.assertFalse((self.licenses / 'jvm-provenance.json').exists())

    def test_rejects_missing_built_provider(self):
        (self.distribution / 'provider-host-4.8.0.jar').unlink()
        with self.assertRaises(FileNotFoundError):
            refresh_provider_host(self.runtime, self.distribution.parent, self.audit)


if __name__ == '__main__':
    unittest.main()
