#!/usr/bin/env python3
"""Bundle common third-party sources and audited notices, without binary caches."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile


def stream_hash(stream):
    digest = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--support', type=Path, required=True)
    parser.add_argument('--repo', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    selected = {}
    for directory, prefix in [(args.support / 'sources', 'sources'),
                              (args.repo / 'packaging/windows-licenses', 'licenses')]:
        if not directory.is_dir():
            raise SystemExit(f'Missing source/notices directory: {directory}')
        for p in directory.rglob('*'):
            if p.is_symlink():
                raise SystemExit(f'Source symlink not allowed: {p}')
            if p.is_file() and '__pycache__' not in p.parts and p.name != '.gitignore':
                selected[f'{prefix}/{p.relative_to(directory).as_posix()}'] = p
    for name in ['RELEASE-PROVENANCE.md', 'jvm-build-provenance.json']:
        selected[name] = args.repo / 'packaging' / name
    args.output.parent.mkdir(parents=True, exist_ok=True)
    inventory = []
    with tarfile.open(args.output, 'w') as tar:
        for name, p in sorted(selected.items()):
            with p.open('rb') as stream:
                digest = stream_hash(stream)
            info = tar.gettarinfo(str(p), arcname=name)
            info.uid = info.gid = info.mtime = 0
            info.uname = info.gname = ''
            info.mode = 0o644
            with p.open('rb') as stream:
                tar.addfile(info, stream)
            inventory.append({'path': name, 'size': p.stat().st_size, 'sha256': digest})
        data = (json.dumps(inventory, indent=2) + '\n').encode()
        info = tarfile.TarInfo('SOURCE-INVENTORY.json')
        info.size = len(data)
        info.mode = 0o644
        tar.addfile(info, io.BytesIO(data))
    with tarfile.open(args.output) as tar:
        for item in inventory:
            stream = tar.extractfile(item['path'])
            assert stream is not None, item['path']
            with stream:
                assert stream_hash(stream) == item['sha256'], item['path']
    with args.output.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    print(f'{digest}  {args.output.name} ({len(inventory)} verified members)')


if __name__ == '__main__':
    main()
