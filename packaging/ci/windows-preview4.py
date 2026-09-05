"""Pinned Preview 4 Windows recovery, without changing shipped dependency bytes.
Runs only on a disposable Windows Actions runner. Never publishes automatically.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

ROOT = Path.cwd()
SOURCE = ROOT / 'source'
EVIDENCE = ROOT / 'evidence'
OUTPUT = ROOT / 'output'
EVIDENCE.mkdir(exist_ok=True)
OUTPUT.mkdir(exist_ok=True)
PIN = 'b2d1f9709d289980af2566108334c11e20b268f1'
BASE_HASH = '116b5847c93ea7fc9ba31f2c2882bf26f958fecb5f4e2ed47d1310a3f985de28'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run(args, cwd=ROOT, log=None, check=True, timeout=1200, env=None):
    print('RUN', args, flush=True)
    p = subprocess.run([str(a) for a in args], cwd=cwd, stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True, errors='replace', timeout=timeout, env=env)
    if log:
        (EVIDENCE / log).write_text(p.stdout, encoding='utf-8')
    print(p.stdout[-10000:], flush=True)
    if check and p.returncode:
        raise RuntimeError(f'{args[0]} exited {p.returncode}')
    return p


def download(url, path):
    print('DOWNLOAD', url, flush=True)
    urllib.request.urlretrieve(url, path)


assert run(['git', 'rev-parse', 'HEAD'], SOURCE).stdout.strip() == PIN
run(['cl'], log='compiler.txt', check=False)
base = ROOT / 'preview3.zip'
download('https://github.com/dcenhance/cloudstream-pc/releases/download/v0.1.0-preview.3/CloudStream-PC-0.1.0-preview.3-Windows-x64.zip', base)
assert sha(base) == BASE_HASH
with zipfile.ZipFile(base) as z:
    assert z.testzip() is None
    z.extractall(ROOT / 'runtime')
runtime = ROOT / 'runtime' / 'CloudStream-PC'
before = {p.relative_to(runtime).as_posix(): sha(p) for p in runtime.rglob('*') if p.is_file()}
old_exe = before['cloudstream.exe']
(runtime / 'cloudstream.exe').unlink()  # An old application executable cannot become output.
deps = ROOT / 'deps'
(deps / 'mpv').mkdir(parents=True)
for header in ['client.h', 'render.h', 'render_gl.h']:
    download('https://raw.githubusercontent.com/mpv-player/mpv/41f6a645068483470267271e1d09966ca3b9f413/include/mpv/' + header, deps / 'mpv' / header)
sdl = deps / 'sdl.zip'
download('https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip', sdl)
with zipfile.ZipFile(sdl) as z:
    assert z.testzip() is None
    z.extractall(deps)
import pefile
for dll, lib in [('libmpv-2.dll', 'mpv'), ('SDL2.dll', 'SDL2')]:
    pe = pefile.PE(str(runtime / dll))
    exports = [s.name.decode('ascii') for s in pe.DIRECTORY_ENTRY_EXPORT.symbols if s.name]
    definition = deps / (lib + '.def')
    definition.write_text('LIBRARY ' + dll + '\nEXPORTS\n' + '\n'.join(exports) + '\n')
    run(['lib', '/nologo', '/machine:x64', '/def:' + str(definition), '/out:' + str(deps / (lib + '.lib'))])
os.environ.update(MPV_INCLUDE=deps.as_posix(), SDL2_INCLUDE=(deps / 'SDL2-2.32.10/include').as_posix(),
                  MPV_LIB=(deps / 'mpv.lib').as_posix(), SDL2_LIB=(deps / 'SDL2.lib').as_posix())


def build(project, name):
    directory = ROOT / ('build-' + name)
    directory.mkdir(exist_ok=True)
    run(['qmake', project, 'CONFIG+=release', 'CONFIG-=debug_and_release'], directory, name + '-qmake.txt')
    run(['nmake', '/NOLOGO'], directory, name + '-build.txt')
    exes = list((directory / 'release').glob('*.exe')) or list(directory.glob('*.exe'))
    assert len(exes) == 1, exes
    return exes[0]


exe = build(SOURCE / 'linux-native/cloudstream-linux.pro', 'app')
shutil.copy2(exe, runtime / 'cloudstream.exe')
assert sha(runtime / 'cloudstream.exe') != old_exe
# The only adaptation to tagged tests is a native fixture executable in place
# of the POSIX shell fixture. Assertions and production source stay unchanged.
fixture = ROOT / 'fixture.cpp'
fixture.write_text('#include <cstdio>\nint main(){puts(R"json({"name":"Regression series","plot":"Details must cover Search.","episodes":[{"name":"First episode","season":1,"episode":1,"data":"fixture"}]})json");}\n')
run(['cl', '/nologo', '/EHsc', '/MT', fixture, '/Fe:' + str(ROOT / 'fixture.exe')])
test = SOURCE / 'linux-native/tests/test_single_window_surfaces.cpp'
text = test.read_text(encoding='utf-8')
start = text.index('        QFile helper(profile.path()')
end = text.index('\n    }', start)
original_fixture = text[start:end]
text = text[:start] + '        const auto helper = qEnvironmentVariable("CLOUDSTREAM_TEST_PROVIDER_FIXTURE");\n        QVERIFY(QFileInfo::exists(helper));\n        qputenv("CLOUDSTREAM_PROVIDER_HOST", helper.toUtf8());' + text[end:]
test.write_text(text, encoding='utf-8')
(EVIDENCE / 'test-fixture-adaptation.diff').write_text(run(['git', 'diff', '--', 'linux-native/tests/test_single_window_surfaces.cpp'], SOURCE).stdout)
# Test DLLs must resolve from the packaged directory before developer Qt.
shutil.copy2(Path('C:/Qt/6.8.3/msvc2022_64/bin/Qt6Test.dll'), runtime / 'Qt6Test.dll')
env = os.environ.copy()
env.update(QT_OPENGL='software', QT_QPA_PLATFORM='windows',
           CLOUDSTREAM_TEST_PROVIDER_FIXTURE=str(ROOT / 'fixture.exe'),
           CLOUDSTREAM_TEST_EVIDENCE=str(EVIDENCE),
           PATH=str(runtime) + os.pathsep + os.environ['PATH'])
results = {}
for name in ['single-window-surfaces', 'home-process-result', 'details-presentation', 'player-command', 'mpv-player-widget']:
    binary = build(SOURCE / ('linux-native/tests/' + name + '.pro'), name)
    deployed_test = runtime / binary.name
    shutil.copy2(binary, deployed_test)
    result = run([deployed_test, '-o', str(EVIDENCE / (name + '.xml')) + ',xml', '-o', str(EVIDENCE / (name + '.txt')) + ',txt'],
                 runtime, name + '-console.txt', check=False, timeout=180, env=env)
    results[name] = result.returncode
    deployed_test.unlink()
    if name != 'mpv-player-widget' and result.returncode:
        raise RuntimeError('Required regression failed: ' + name)
(runtime / 'Qt6Test.dll').unlink()
# Genuine packaged application event loop, packaged JVM and FFmpeg.
clean_env = env.copy()
clean_env['PATH'] = str(runtime) + os.pathsep + str(Path(os.environ['SystemRoot']) / 'System32')
clean_env.pop('CLOUDSTREAM_PROVIDER_HOST', None)
run([runtime / 'cloudstream.exe', '--smoke-test'], runtime, 'portable-smoke.txt', timeout=45, env=clean_env)
run([runtime / 'runtime/bin/java.exe', '-version'], runtime, 'java-version.txt', timeout=60, env=clean_env)
helper = run([runtime / 'runtime/bin/java.exe', '-cp', str(runtime / 'provider-host/lib/*'), 'com.lagradost.cloudstream3.linux.host.MainKt'], runtime, 'provider-help.txt', check=False, timeout=60, env=clean_env)
assert 'Usage: cloudstream-provider-host list' in helper.stdout
run([runtime / 'ffmpeg.exe', '-version'], runtime, 'ffmpeg-version.txt', timeout=30, env=clean_env)
after = {p.relative_to(runtime).as_posix(): sha(p) for p in runtime.rglob('*') if p.is_file()}
assert before.keys() == after.keys()
assert [p for p in before if before[p] != after[p]] == ['cloudstream.exe']
(EVIDENCE / 'runtime-manifest.json').write_text(json.dumps(after, indent=2))
(EVIDENCE / 'build-evidence.json').write_text(json.dumps({'source_commit': PIN, 'base_zip_sha256': BASE_HASH,
    'old_exe_sha256': old_exe, 'exe_sha256': after['cloudstream.exe'], 'changed_runtime_files': ['cloudstream.exe'],
    'tests': results, 'physical_gpu_tested': False, 'test_fixture': 'native C++ instead of /bin/sh; assertions unchanged'}, indent=2))
portable = OUTPUT / 'CloudStream-PC-0.1.0-preview.4-Windows-x64.zip'
run([sys.executable, SOURCE / 'packaging/build-windows-portable.py', runtime, portable], log='portable-package.txt')
if not shutil.which('makensis'):
    run(['choco', 'install', 'nsis', '-y', '--no-progress'])
    os.environ['PATH'] += os.pathsep + 'C:/Program Files (x86)/NSIS'
installer = OUTPUT / 'CloudStream-PC-0.1.0-preview.4-Windows-x64-Setup.exe'
run([sys.executable, SOURCE / 'packaging/build-windows-installer.py', runtime, installer, '--version', '0.1.0-preview.4'], log='installer-package.txt')
installed = ROOT / 'installed test'
run([installer, '/S', '/D=' + str(installed)], log='install.txt', timeout=180)
assert sha(installed / 'cloudstream.exe') == after['cloudstream.exe']
import winreg
key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Uninstall\CloudStreamPC')
assert winreg.QueryValueEx(key, 'DisplayVersion')[0] == '0.1.0-preview.4'
key.Close()
run([installed / 'cloudstream.exe', '--smoke-test'], installed, 'installed-smoke.txt', timeout=45, env=clean_env)
sentinel = installed / 'user-added-sentinel.txt'
sentinel.write_text('preserve me')
run([installed / 'Uninstall.exe', '/S', '_?=' + str(installed)], log='uninstall.txt', timeout=180)
assert not (installed / 'cloudstream.exe').exists()
assert sentinel.read_text() == 'preserve me'
try:
    winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Uninstall\CloudStreamPC')
except FileNotFoundError:
    pass
else:
    raise AssertionError('Uninstall registry entry remains')
(EVIDENCE / 'installer-lifecycle.json').write_text(json.dumps({'install_version': '0.1.0-preview.4', 'exe_matches_portable': True,
    'installed_smoke_passed': True, 'uninstalled_shipped_exe': True, 'preserved_user_file': True, 'removed_uninstall_registry': True}, indent=2))
(OUTPUT / 'WINDOWS-SHA256SUMS').write_text(''.join(sha(p) + '  ' + p.name + '\n' for p in [portable, installer]))
print('WINDOWS PACKAGING AND REQUIRED REGRESSIONS PASSED', flush=True)
