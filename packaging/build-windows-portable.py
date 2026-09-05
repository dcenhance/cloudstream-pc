#!/usr/bin/env python3
"""Create and validate a portable Windows ZIP from a deployed runtime."""
import argparse
import hashlib
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    root = args.runtime.resolve()
    required = ('cloudstream.exe', 'runtime/bin/java.exe', 'libmpv-2.dll',
                'SDL2.dll', 'platforms/qwindows.dll', 'vulkan-1.dll',
                'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll', 'LICENSE')
    for name in required:
        if not (root / name).is_file():
            parser.error(f'Missing portable dependency: {name}')
    files = sorted(p for p in root.rglob('*') if p.is_file())
    for path in files:
        if path.is_symlink():
            parser.error(f'Symlink in runtime: {path}')
        rel = path.relative_to(root)
        if any(part in ('verification', '.git', '.env') for part in rel.parts):
            parser.error(f'Private/build artifact in runtime: {rel}')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in files:
            archive.write(path, 'CloudStream-PC/' + path.relative_to(root).as_posix())
    with zipfile.ZipFile(args.output) as archive:
        bad = archive.testzip()
        if bad:
            raise RuntimeError(f'Archive CRC failure: {bad}')
        assert len(archive.namelist()) == len(files)
    with args.output.open('rb') as stream:
        checksum = hashlib.file_digest(stream, 'sha256').hexdigest()
    print(f'{checksum}  {args.output.name}')
    print(f'Validated {len(files)} files; {args.output.stat().st_size} bytes')


if __name__ == '__main__':
    main()
