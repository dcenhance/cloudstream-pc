#!/usr/bin/env python3
"""Verify package structure, artifact equality and bounded GUI/helper launches."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import shutil
import tarfile

REPO = Path(__file__).resolve().parents[2]
OUT = Path(os.environ.get('CLOUDSTREAM_RELEASE_OUT', str(REPO / 'dist-linux')))
V = OUT / 'verification'
V.mkdir(exist_ok=True)
CONTAINER = os.environ.get('CLOUDSTREAM_CONTAINER', 'cloudstream-linux-packager')

def command(args, name, accepted=(0,)):
    p = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=90)
    (V / name).write_text(p.stdout)
    assert p.returncode in accepted, (name, p.returncode, p.stdout[-1000:])
    return {'exit_code': p.returncode, 'log': name}

result = {}
# Always extract the just-built packages, never verify stale extraction trees.
for name in ('deb', 'rpm', 'appimage'):
    if (V/name).exists():
        shutil.rmtree(V/name)
    (V/name).mkdir()
result['fresh-extraction'] = command(['podman', 'exec', CONTAINER, 'bash', '-lc', 'set -euo pipefail; dpkg-deb -x /out/cloudstream-pc_0.1.0~preview.2_amd64.deb /out/verification/deb; cd /out/verification/rpm; rpm2cpio /out/cloudstream-pc-0.1.0-0.preview.2.x86_64.rpm | cpio -idm; cd /out/verification/appimage; /out/CloudStream-PC-0.1.0-preview.2-x86_64-system-runtime.AppImage --appimage-extract'], 'fresh-extraction.log')
result['rpm-requirements'] = command(['rpm', '-qp', '--requires', str(OUT/'cloudstream-pc-0.1.0-0.preview.2.x86_64.rpm')], 'rpm-requirements.txt')
assert 'qt6-qtsvg >= 6.4.2' in (V/'rpm-requirements.txt').read_text()
for suffix, args in [('deb', ['podman','exec',CONTAINER,'dpkg-deb','-I','/out/cloudstream-pc_0.1.0~preview.2_amd64.deb']), ('rpm', ['rpm','-qpi',str(OUT/'cloudstream-pc-0.1.0-0.preview.2.x86_64.rpm')])]:
    result[suffix+'-inspection'] = command(args, suffix+'-inspection.txt')
result['rpm-dependency-transaction-test-nobara44'] = command(['rpm','-U','--test',str(OUT/'cloudstream-pc-0.1.0-0.preview.2.x86_64.rpm')], 'rpm-host-transaction-test.log')
expected = hashlib.sha256((OUT / 'cloudstream-linux-ubuntu24.04').read_bytes()).hexdigest()
paths = {'deb': V/'deb', 'rpm': V/'rpm', 'appimage': V/'appimage/squashfs-root'}
for name, root in paths.items():
    assert hashlib.sha256((root/'usr/bin/cloudstream-linux').read_bytes()).hexdigest() == expected
    jars = list((root/'usr/libexec/cloudstream/provider-host/lib').glob('*.jar'))
    assert len(jars) == 61
    for jar in jars:
        staged = OUT/'build-support/root/usr/libexec/cloudstream/provider-host/lib'/jar.name
        assert hashlib.sha256(jar.read_bytes()).digest() == hashlib.sha256(staged.read_bytes()).digest()
    with tarfile.open(root/'usr/share/doc/cloudstream-pc/cloudstream-corresponding-source.tar.gz') as archive:
        for relative in ('linux-native/main.cpp', 'linux-native/app/PackagedRuntimeEnvironment.h'):
            member = archive.extractfile('cloudstream/'+relative)
            assert member is not None
            assert member.read() == (REPO/relative).read_bytes()
    assert 'libqt6svg6 (>= 6.4.2)' in (V/'deb-inspection.txt').read_text()
    assert 'not bundle Qt' in (root/'usr/share/doc/cloudstream-pc/README.md').read_text()
    assert (root/'usr/share/doc/cloudstream-pc/LICENSE').is_file()
    assert (root/'usr/share/doc/cloudstream-pc/cloudstream-corresponding-source.tar.gz').is_file()
    croot = '/out/' + str(root.relative_to(OUT))
    state = '/tmp/cloudstream-verify-' + name
    shell = f'mkdir -p {state}; cd /tmp; HOME={state} XDG_CONFIG_HOME={state}/config XDG_DATA_HOME={state}/data XDG_CACHE_HOME={state}/cache timeout -k 5 10 xvfb-run -a {croot}/usr/bin/cloudstream-pc'
    result[name+'-extracted-launch'] = command(['podman','exec',CONTAINER,'bash','-lc',shell],name+'-extracted-launch.log',(124,))
    result[name+'-provider-operation'] = command(['podman','exec',CONTAINER,croot+'/usr/libexec/cloudstream/provider-host/bin/cloudstream-provider-host','repository-candidates',croot+'/usr/libexec/cloudstream/provider-host/lib/provider-host-4.8.0.jar'],name+'-provider-operation.json')
result['appimage-executable-extract-and-run'] = command(['podman','exec',CONTAINER,'bash','-lc','cd /tmp; HOME=/tmp/cloudstream-verify-appimage timeout -k 5 10 xvfb-run -a /out/CloudStream-PC-0.1.0-preview.2-x86_64-system-runtime.AppImage --appimage-extract-and-run'],'appimage-direct-launch.log',(124,))
result['elf_sha256'] = expected
result['jar_count_per_package'] = 61
result['baseline'] = 'Ubuntu 24.04 amd64, glibc 2.39, Qt 6.4.2; system runtime dependencies'
result['limitations'] = ['No clean graphical desktop install test', 'No clean RPM distro install', 'No packaged provider streaming/playback/hardware/controller regression test', 'AppImage requires installed Qt/mpv/SDL/Java/FFmpeg runtimes']
result['artifacts'] = [{'file':p.name,'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(OUT.glob('*')) if p.suffix in {'.AppImage','.deb','.rpm'}]
(V/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
