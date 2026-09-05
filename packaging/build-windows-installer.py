#!/usr/bin/env python3
"""Package an already deployed Windows runtime using NSIS (Linux or Windows)."""
import argparse
import pathlib
import subprocess
import tempfile


def nsis_quote(value):
    value = str(value)
    if '\n' in value or '\r' in value:
        raise ValueError('Newlines are not allowed in package paths')
    return value.replace('$', '$$').replace('"', '$\\"')


def nsis_file_pattern(runtime):
    """File directives need host-native separators, including the wildcard."""
    return nsis_quote(runtime / '*')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--version', default='0.1.0-preview.2')
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    output = args.output.resolve()
    for required in ('cloudstream.exe', 'runtime/bin/java.exe', 'platforms/qwindows.dll'):
        if not (runtime / required).is_file():
            parser.error(f'Missing runtime file: {required}')
    output.parent.mkdir(parents=True, exist_ok=True)
    paths = sorted(runtime.rglob('*'))
    if any(p.is_symlink() for p in paths):
        parser.error('Runtime must not contain symlinks')
    files = [p for p in paths if p.is_file()]
    # Explicit deletes preserve files added by the user after installation.
    deletes = '\n'.join('Delete "$INSTDIR\\' + nsis_quote(str(p.relative_to(runtime)).replace('/', '\\')) + '"' for p in files)
    dirs = sorted((p for p in paths if p.is_dir()), key=lambda p: len(p.parts), reverse=True)
    removes = '\n'.join('RMDir "$INSTDIR\\' + nsis_quote(str(p.relative_to(runtime)).replace('/', '\\')) + '"' for p in dirs)
    script = '''Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
Name "CloudStream PC"
VIProductVersion "0.1.0.0"
VIAddVersionKey /LANG=1033 "ProductName" "CloudStream PC"
VIAddVersionKey /LANG=1033 "CompanyName" "dcenhance"
VIAddVersionKey /LANG=1033 "FileDescription" "CloudStream PC per-user installer"
VIAddVersionKey /LANG=1033 "FileVersion" "@VERSION@"
VIAddVersionKey /LANG=1033 "LegalCopyright" "CloudStream contributors; GPL v3"
!define MUI_ICON "@ICON@"
!define MUI_UNICON "@ICON@"
OutFile "@OUTPUT@"
InstallDir "$LOCALAPPDATA\\Programs\\CloudStream PC"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "CloudStream PC requires 64-bit Windows."
    Abort
  ${EndIf}
FunctionEnd
Section "CloudStream PC"
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "@FILE_PATTERN@"
  WriteUninstaller "$INSTDIR\\Uninstall.exe"
  CreateDirectory "$SMPROGRAMS\\CloudStream PC"
  CreateShortcut "$SMPROGRAMS\\CloudStream PC\\CloudStream PC.lnk" "$INSTDIR\\cloudstream.exe"
  CreateShortcut "$DESKTOP\\CloudStream PC.lnk" "$INSTDIR\\cloudstream.exe"
  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CloudStreamPC" "DisplayName" "CloudStream PC"
  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CloudStreamPC" "DisplayVersion" "@VERSION@"
  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CloudStreamPC" "Publisher" "dcenhance"
  WriteRegStr HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CloudStreamPC" "UninstallString" '$\\"$INSTDIR\\Uninstall.exe$\\"'
  WriteRegDWORD HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CloudStreamPC" "NoModify" 1
  WriteRegDWORD HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CloudStreamPC" "NoRepair" 1
SectionEnd
Section "Uninstall"
  SetShellVarContext current
@DELETES@
  Delete "$INSTDIR\\Uninstall.exe"
@REMOVES@
  RMDir "$INSTDIR"
  Delete "$DESKTOP\\CloudStream PC.lnk"
  Delete "$SMPROGRAMS\\CloudStream PC\\CloudStream PC.lnk"
  RMDir "$SMPROGRAMS\\CloudStream PC"
  DeleteRegKey HKCU "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CloudStreamPC"
SectionEnd
'''
    for key, value in {'OUTPUT': nsis_quote(output), 'FILE_PATTERN': nsis_file_pattern(runtime), 'ICON': nsis_quote(pathlib.Path(__file__).with_name('cloudstream.ico').resolve()), 'VERSION': nsis_quote(args.version), 'DELETES': deletes, 'REMOVES': removes}.items():
        script = script.replace('@' + key + '@', value)
    with tempfile.TemporaryDirectory(prefix='cloudstream-nsis-') as tmp:
        path = pathlib.Path(tmp) / 'installer.nsi'
        path.write_text(script, encoding='utf-8')
        subprocess.run(['makensis', '-V2', str(path)], check=True)
    print(output)


if __name__ == '__main__':
    main()
