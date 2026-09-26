"""Refresh the application-owned provider host in an audited Windows runtime."""

import hashlib
import json
from pathlib import Path
import shutil


PROVIDER_JAR = 'provider-host-4.8.0.jar'


def digest(path: Path) -> str:
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def refresh_provider_host(runtime: Path, distribution: Path, audit_path: Path) -> list[str]:
    built = distribution / 'lib' / PROVIDER_JAR
    if not built.is_file():
        raise FileNotFoundError(built)
    jars = runtime / 'provider-host/lib'
    audit = json.loads(audit_path.read_text(encoding='utf-8'))
    names = {entry['jar'] for entry in audit}
    if names != {path.name for path in jars.glob('*.jar')}:
        raise ValueError('Windows provider-host dependency inventory differs from audit')
    for entry in audit:
        if entry['jar'] == PROVIDER_JAR:
            entry['sha256'] = digest(built)
        elif digest(jars / entry['jar']) != entry['sha256']:
            raise ValueError('Unaudited Windows dependency: ' + entry['jar'])
    if PROVIDER_JAR not in names:
        raise ValueError('Provider host is missing from JVM audit')
    shutil.copy2(built, jars / PROVIDER_JAR)
    (runtime / 'licenses/jvm-provenance.json').write_text(
        json.dumps(audit, indent=2) + '\n', encoding='utf-8')
    return [f'provider-host/lib/{PROVIDER_JAR}', 'licenses/jvm-provenance.json']
