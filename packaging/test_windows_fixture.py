import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


FIXTURE = Path(__file__).parent / 'ci/provider-fixture.cpp'


@unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
class WindowsProviderFixtureTests(unittest.TestCase):
    def test_native_fixture_covers_both_details_and_episode_protocol(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / 'fixture'
            subprocess.run(['c++', '-std=c++17', str(FIXTURE), '-o', str(binary)], check=True)
            result = subprocess.run([binary, 'load'], capture_output=True, text=True, check=True)
            self.assertEqual(json.loads(result.stdout)['name'], 'Regression series')
            env = os.environ.copy()
            env['CLOUDSTREAM_EPISODE_VIDEO'] = 'http://127.0.0.1:8099/episode.mp4'
            result = subprocess.run([binary, 'load'], capture_output=True, text=True, check=True, env=env)
            self.assertEqual(len(json.loads(result.stdout)['episodes']), 2)
            result = subprocess.run([binary, 'sources', 'fixture.jar', 'provider', 'series', 'ep2'],
                                    capture_output=True, text=True, check=True, env=env)
            links = json.loads(result.stdout)['links']
            self.assertEqual([link['source'] for link in links], ['Primary', 'Alternate'])
            self.assertEqual(links[1]['url'], env['CLOUDSTREAM_EPISODE_VIDEO'] + '?alternate')


if __name__ == '__main__':
    unittest.main()
