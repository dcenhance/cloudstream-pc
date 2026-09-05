#!/usr/bin/env python3
"""Package real Ubuntu-built ELF with system runtimes, not host Nobara libraries."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile

REPO = Path(__file__).resolve().parents[2]
ORIGINAL = Path(os.environ.get('CLOUDSTREAM_ORIGINAL', str(REPO)))
OUT = Path(os.environ.get('CLOUDSTREAM_RELEASE_OUT', str(REPO / 'dist-linux')))
CONTAINER = os.environ.get('CLOUDSTREAM_CONTAINER', 'cloudstream-linux-packager')
WORK = OUT / 'build-support'
ROOT = WORK / 'root'
VERSION = '0.1.0-preview.3'

def run(*args):
    subprocess.run(args, check=True)

def inside(command):
    run('podman', 'exec', CONTAINER, 'bash', '-lc', command)

def put(relative, content, mode=0o644):
    p = ROOT / relative
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(content)
    p.chmod(mode)

def copy(source, relative):
    p = ROOT / relative
    p.parent.mkdir(parents=True, exist_ok=True)
    if source.is_dir():
        shutil.copytree(source, p, dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    else:
        shutil.copy2(source, p)

assert (OUT / 'cloudstream-linux-ubuntu24.04').is_file(), 'Build container ELF first'
if ROOT.exists():
    shutil.rmtree(ROOT)
ROOT.mkdir(parents=True)
copy(OUT / 'cloudstream-linux-ubuntu24.04', 'usr/bin/cloudstream-linux')
(ROOT / 'usr/bin/cloudstream-linux').chmod(0o755)
put('usr/bin/cloudstream-pc', '#!/bin/sh\nset -eu\nHERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexport CLOUDSTREAM_PROVIDER_HOST="$HERE/../libexec/cloudstream/provider-host/bin/cloudstream-provider-host"\nexec "$HERE/cloudstream-linux" "$@"\n', 0o755)
copy(ORIGINAL / 'provider-host/build/install/cloudstream-provider-host', 'usr/libexec/cloudstream/provider-host')
for p in (ROOT / 'usr/libexec/cloudstream/provider-host/bin').glob('*.bat'):
    p.unlink()
(ROOT / 'usr/libexec/cloudstream/provider-host/bin/cloudstream-provider-host').chmod(0o755)
put('usr/share/applications/io.github.recloudstream.cloudstream-pc.desktop', '[Desktop Entry]\nType=Application\nName=CloudStream PC\nComment=CloudStream native desktop preview\nExec=cloudstream-pc\nIcon=io.github.recloudstream.cloudstream\nTerminal=false\nCategories=AudioVideo;Video;\nStartupWMClass=cloudstream-linux\n')
copy(REPO / 'linux-native/packaging/io.github.recloudstream.cloudstream.svg', 'usr/share/icons/hicolor/scalable/apps/io.github.recloudstream.cloudstream.svg')
doc = Path('usr/share/doc/cloudstream-pc')
copy(REPO / 'LICENSE', str(doc / 'LICENSE'))
copy(REPO / 'packaging/linux/README.md', str(doc / 'README.md'))
# Full corresponding local source for the GUI and tested JVM modules, including build files.
source_tar = ROOT / doc / 'cloudstream-corresponding-source.tar.gz'
with tarfile.open(source_tar, 'w:gz') as t:
    # Only version-controlled project files, never ignored files from the
    # developer's original workspace. Include packaging scripts being prepared
    # for this release even before the packaging commit is made. Explicitly
    # include the new runtime helper without collecting unrelated untracked files.
    tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=REPO).decode().split('\0')
    pending = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard', '-z', 'packaging/linux', 'linux-native/app/PackagedRuntimeEnvironment.h'], cwd=REPO).decode().split('\0')
    allowed = {'app', 'docs', 'linux', 'linux-native', 'provider-host', 'library', 'gradle', 'packaging'}
    roots = {'gradlew', 'gradlew.bat', 'build.gradle.kts', 'settings.gradle.kts', 'gradle.properties', 'LICENSE'}
    for name in sorted(set(tracked + pending)):
        if not name or (name.split('/')[0] not in allowed and name not in roots):
            continue
        f = REPO / name
        if f.is_file() and not f.is_symlink():
            t.add(f, arcname=f'cloudstream/{name}', recursive=False)
# Reuse the audited, byte-identical JVM dependency notices, not native Windows licenses.
audit = REPO / 'packaging/windows-licenses'
for name in ['jvm-embedded','jvm-poms','jvm-source-licenses','jvm-provenance.json','jvm-inventory.json','jvm-shaded-followup']:
    copy(audit / name, str(doc / 'jvm-licenses' / name))
for name in ['Apache-2.0.txt','MPL-2.0.txt','asm-license.html','protobuf-LICENSE','ksoup-LICENSE']:
    copy(audit / 'upstream' / name, str(doc / 'jvm-licenses' / name))
source_support = OUT.parent / 'windows-source-support/sources'
for name in ['NewPipeExtractor-v0.26.3.tar.gz','NiceHttp-0.4.18.tar.gz','rhino-1.8.1.tar.gz','nanojson-e9d656.tar.gz']:
    copy(source_support / name, str(doc / 'third-party-sources' / name))
copy(source_support / 'jvm', str(doc / 'third-party-sources/jvm'))
provenance = json.loads((audit / 'jvm-provenance.json').read_text())
jar_dir = ROOT / 'usr/libexec/cloudstream/provider-host/lib'
verified = []
for item in provenance:
    jar = jar_dir / item['jar']
    if jar.is_file():
        digest = hashlib.sha256(jar.read_bytes()).hexdigest()
        assert digest == item['sha256'], f'JVM audit mismatch: {jar.name}'
        verified.append(jar.name)
assert len(verified) == len(list(jar_dir.glob('*.jar'))), 'Every JAR must match the audit'
put(str(doc / 'THIRD-PARTY-NOTICES.txt'), 'CloudStream: GPL-3.0; see LICENSE and cloudstream-corresponding-source.tar.gz.\nJVM dependencies: see jvm-licenses/jvm-provenance.json and preserved embedded notices.\nGPL NewPipeExtractor and NiceHttp and MPL Rhino corresponding source is included.\nAll available JVM source JARs are included under third-party-sources/jvm.\nNative Qt, libmpv, SDL2, FFmpeg, glibc, Java and drivers are SYSTEM dependencies, not bundled.\nAppImage runtime: upstream type2-runtime license retained in AppImage-specific documentation.\n')
# DEB: all actual GUI and helper system requirements are explicit.
debroot = WORK / 'debroot'
if debroot.exists():
    shutil.rmtree(debroot)
shutil.copytree(ROOT, debroot)
(debroot / 'DEBIAN').mkdir()
(debroot / 'DEBIAN/control').write_text('Package: cloudstream-pc\nVersion: 0.1.0~preview.3\nArchitecture: amd64\nMaintainer: dcenhance <252102103+dcenhance@users.noreply.github.com>\nSection: video\nPriority: optional\nDepends: libc6 (>= 2.39), libstdc++6 (>= 13.2), libgcc-s1, libqt6core6t64 (>= 6.4.2), libqt6widgets6 (>= 6.4.2), libqt6gui6 (>= 6.4.2), libqt6network6 (>= 6.4.2), libqt6opengl6 (>= 6.4.2), libqt6openglwidgets6 (>= 6.4.2), libqt6concurrent6 (>= 6.4.2), libqt6svg6 (>= 6.4.2), libmpv2 (>= 0.37), libsdl2-2.0-0 (>= 2.30), qt6-qpa-plugins, qt6-wayland, qt6-image-formats-plugins, openjdk-17-jre-headless | java17-runtime-headless, ffmpeg (>= 6), ca-certificates\nDescription: CloudStream PC native desktop preview (system runtime)\n Ubuntu 24.04 amd64 baseline; Qt, mpv, Java and FFmpeg are not bundled.\n')
inside('dpkg-deb --root-owner-group --build /out/build-support/debroot /out/cloudstream-pc_0.1.0.preview.3_amd64.deb')
# RPM auto-generated ELF requirements are preserved, plus helper/dlopen dependencies.
spec = WORK / 'cloudstream-pc.spec'
spec.write_text('''%global __os_install_post %{nil}
%global _build_id_links none
Name: cloudstream-pc
Version: 0.1.0
Release: 0.preview.3
Summary: CloudStream PC native desktop preview (system runtime)
Packager: dcenhance
License: GPL-3.0-only AND Apache-2.0 AND MIT AND MPL-2.0 AND BSD-3-Clause
BuildArch: x86_64
Requires: glibc >= 2.39
Requires: qt6-qtbase >= 6.4.2
Requires: qt6-qtbase-gui >= 6.4.2
Requires: qt6-qtwayland
Requires: qt6-qtimageformats
Requires: qt6-qtsvg >= 6.4.2
Requires: libmpv.so.2()(64bit)
Requires: libSDL2-2.0.so.0()(64bit)
Requires: java-headless >= 17
Requires: (ffmpeg >= 6 or ffmpeg-free >= 6)
Requires: ca-certificates
%description
Qt desktop GUI and JVM provider host. Built on Ubuntu 24.04 with Qt 6.4.2.
System Qt, libmpv, SDL2, Java and FFmpeg are required, not bundled.
%prep
%build
%install
mkdir -p %{buildroot}
cp -a /out/build-support/root/. %{buildroot}/
%files
/usr/bin/cloudstream-linux
/usr/bin/cloudstream-pc
/usr/libexec/cloudstream
/usr/share/applications/io.github.recloudstream.cloudstream-pc.desktop
/usr/share/icons/hicolor/scalable/apps/io.github.recloudstream.cloudstream.svg
/usr/share/doc/cloudstream-pc
''')
inside('rpmbuild -bb --define "_topdir /out/build-support/rpmbuild" /out/build-support/cloudstream-pc.spec')
for p in (WORK / 'rpmbuild/RPMS/x86_64').glob('*.rpm'):
    shutil.copy2(p, OUT / p.name)
# Actual executable type-2 AppImage. Runtime supplied by upstream, SquashFS by distro.
appdir = WORK / 'AppDir'
if appdir.exists():
    shutil.rmtree(appdir)
shutil.copytree(ROOT, appdir)
(appdir / 'AppRun').write_text('#!/bin/sh\nset -eu\nAPPDIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$APPDIR/usr/bin/cloudstream-pc" "$@"\n')
(appdir / 'AppRun').chmod(0o755)
shutil.copy2(ROOT / 'usr/share/applications/io.github.recloudstream.cloudstream-pc.desktop', appdir / 'cloudstream-pc.desktop')
shutil.copy2(ROOT / 'usr/share/icons/hicolor/scalable/apps/io.github.recloudstream.cloudstream.svg', appdir / 'io.github.recloudstream.cloudstream.svg')
(appdir / '.DirIcon').symlink_to('io.github.recloudstream.cloudstream.svg')
shutil.copy2(WORK / 'AppImage-runtime-LICENSE', appdir / doc / 'AppImage-runtime-LICENSE')
shutil.copy2(WORK / 'type2-runtime-source.tar.gz', appdir / doc / 'type2-runtime-source.tar.gz')
shutil.copytree(WORK / 'runtime-sources', appdir / doc / 'runtime-sources')
inside('mksquashfs /out/build-support/AppDir /out/build-support/app.squashfs -noappend -all-root -comp gzip -processors 4')
appimage = OUT / f'CloudStream-PC-{VERSION}-x86_64-system-runtime.AppImage'
with appimage.open('wb') as out:
    for p in [WORK / 'runtime-x86_64', WORK / 'app.squashfs']:
        with p.open('rb') as src:
            shutil.copyfileobj(src, out)
appimage.chmod(0o755)
shutil.copy2(REPO / 'packaging/linux/README.md', OUT / 'README-LINUX.md')
artifacts = sorted([*OUT.glob('*.deb'), *OUT.glob('*.rpm'), *OUT.glob('*.AppImage')])
assert len(artifacts) == 3
(OUT / 'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in artifacts))
(WORK / 'jvm-audit-match.json').write_text(json.dumps({'matched_third_party_jars': len(verified), 'jars': verified}, indent=2))
print((OUT / 'SHA256SUMS').read_text())
