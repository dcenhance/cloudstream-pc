# CloudStream Windows port

The Windows port uses the same Qt 6 frontend, isolated JVM provider host, artwork/download pipeline, and embedded libmpv player as the Linux build.

## Requirements

- Windows 10 or newer
- Qt 6 for MSVC, with `qmake`, `windeployqt`, and `nmake` available
- Java 17 available as `java`, with `JAVA_HOME` set; the runtime is bundled
- MSVC build tools
- Windows development packages for libmpv and SDL2

Set these environment variables in a **Developer PowerShell for VS** before building:

```powershell
$env:MPV_INCLUDE = 'C:\libs\mpv\include'
$env:SDL2_INCLUDE = 'C:\libs\SDL2\include'
$env:MPV_LIB = 'C:\libs\mpv\lib\mpv-2.lib'
$env:SDL2_LIB = 'C:\libs\SDL2\lib\SDL2.lib'
$env:MPV_DLL = 'C:\libs\mpv\bin\mpv-2.dll'
$env:SDL2_DLL = 'C:\libs\SDL2\bin\SDL2.dll'
```

Run:

```powershell
cd linux-native
.\build-windows.ps1
```

The package is written to `dist-windows`. It contains Qt, the CloudStream executable, provider-host distribution, Java runtime, libmpv, and SDL2. `-JavaRuntime` can select a separate Windows Java runtime instead of copying `JAVA_HOME`.

The build also requires app-local Visual C++ runtime DLLs. It uses `VCToolsRedistDir\x64\Microsoft.VC143.CRT` from the developer shell, or an explicit `-MsvcRuntimeDirectory`. Merely shipping `vc_redist.x64.exe` does not make a portable ZIP self-contained. The script rejects missing CRT dependencies before building.

Use `-ThirdPartyNoticesDirectory` to include collected dependency notices. The project's GPL license is copied automatically; this is not a substitute for the licenses and corresponding source required by each bundled dependency.

## Portable ZIP and installer

With a complete deployed runtime, Python 3.11+ and NSIS available:

```sh
python3 packaging/build-windows-portable.py dist-windows CloudStream-PC-Windows-x64.zip
python3 packaging/build-windows-installer.py dist-windows CloudStream-PC-Windows-x64-Setup.exe
python3 packaging/test_windows_installer.py
```

Run these commands from the repository root. The installer is per-user, requires no administrator elevation, creates Start Menu and desktop shortcuts, and registers an uninstaller. Uninstall deletes only packaged files; application profiles and user-added files are retained. Close CloudStream before installing an update or uninstalling.

The packages are not Authenticode-signed. Windows may show an unknown-publisher/SmartScreen warning. Verify the release URL and SHA-256 checksums; do not disable antivirus or system-wide security checks.

The Java provider host is platform-independent. To reuse an already-built Gradle `installDist` distribution, pass `-ProviderHostInstall C:\path\to\cloudstream-provider-host`; this avoids configuring the Android Gradle project inside the Windows VM. Without this argument, the script invokes the root Gradle build, which also configures Android modules and requires their prerequisites.

Pass additional DLL dependencies with `-AdditionalRuntimeDlls`; current shinchiro libmpv builds require `vulkan-1.dll`. Pass `-FfmpegExecutable C:\path\to\ffmpeg.exe` to bundle adaptive-download support. Keep third-party licenses and corresponding source obligations when redistributing these components.

GPU-less Windows VMs need a current software renderer: Qt 6.8.3's bundled Mesa 11 fallback produced blank libmpv frames in testing. `-SoftwareOpenGLDirectory C:\path\to\mesa\x64` bundles `opengl32.dll` as `opengl32sw.dll` plus `libgallium_wgl.dll`. Mesa 26.2.0 passed the player suite in QEMU. Set `QT_OPENGL=software` for a GPU-less VM; physical PCs can retain automatic graphics selection.

For libmpv archives that provide only a MinGW import library, generate an MSVC `.lib` using the DLL's actual exports and `lib.exe /def:mpv.def /machine:x64 /out:mpv.lib`. The `LIBRARY` line must name the exact runtime DLL, such as `libmpv-2.dll`.

## Runtime storage

Windows uses Qt's standard application locations rather than Linux XDG paths:

- Configuration: `%APPDATA%\CloudStream`
- Data/extensions/history: `%LOCALAPPDATA%\CloudStream`
- Cache/logs: `%LOCALAPPDATA%\CloudStream`

The provider host is launched from the packaged `provider-host` directory. `CLOUDSTREAM_PROVIDER_HOST` can override it for development.

## Windows boundaries

- DRM-protected streams, Android WebViews, Android account integrations, and challenge-solving remain unsupported on Windows for the same compatibility reasons as Linux.
- Build and runtime verification must run on Windows, including when hosted in a Linux QEMU/KVM VM. A successful Linux build alone is not Windows validation.
