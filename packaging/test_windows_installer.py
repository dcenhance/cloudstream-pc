"""Unit checks for installer source escaping; VM validates install/uninstall."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('installer', Path(__file__).with_name('build-windows-installer.py'))
assert spec is not None and spec.loader is not None
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallerQuotingTests(unittest.TestCase):
    def test_spaces_and_unicode_are_preserved(self):
        self.assertEqual(installer.nsis_quote('A folder/日本語'), 'A folder/日本語')

    def test_variable_interpolation_is_escaped(self):
        self.assertEqual(installer.nsis_quote('$INSTDIR'), '$$INSTDIR')

    def test_quotes_are_escaped(self):
        self.assertEqual(installer.nsis_quote('a"b'), 'a$\\"b')

    def test_line_injection_is_rejected(self):
        for value in ('a\nb', 'a\rb'):
            with self.assertRaises(ValueError):
                installer.nsis_quote(value)


if __name__ == '__main__':
    unittest.main()
