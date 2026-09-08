"""Shared release identity; keep VERSION as the only hand-edited version."""
import json
import re
from pathlib import Path

VERSION_FILE = Path(__file__).resolve().parents[1] / 'linux-native' / 'VERSION.txt'


def release_version():
    version = VERSION_FILE.read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-[a-zA-Z0-9]+(?:\.[a-zA-Z0-9]+)*)?', version):
        raise ValueError('Invalid release VERSION')
    return version


def identity(package, version=None):
    if package not in {'windows-setup', 'windows-zip', 'appimage', 'deb', 'rpm'}:
        raise ValueError('Unknown package identity')
    current = release_version()
    if version is not None and version != current:
        raise ValueError('Package version must match linux-native/VERSION.txt; rebuild first')
    return json.dumps({'version': current, 'package': package}, separators=(',', ':')) + '\n'


def require_build_version(path):
    if Path(path).read_text(encoding='utf-8').strip() != release_version():
        raise ValueError('Compiled build and package VERSION differ; rebuild first')
