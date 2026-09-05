# CloudStream PC

CloudStream for **Linux and Windows**, maintained by [dcenhance](https://github.com/dcenhance). A shared Qt 6 desktop interface with an out-of-process JVM extension runtime and embedded libmpv playback.

An independent desktop adaptation of [recloudstream/cloudstream](https://github.com/recloudstream/cloudstream), not an official upstream desktop release.

## Contents

- [Downloads and installation](#downloads-and-installation)
- [Platforms](#platforms)
- [Features](#features)
- [Build](#build)
- [Project layout](#project-layout)
- [Compatibility and security](#compatibility-and-security)
- [License](#license)

## Platforms

| Platform | Status |
| --- | --- |
| Linux x86-64 | Built and tested on Nobara/KDE Wayland |
| Windows x86-64 | Built with MSVC 2022 and Qt 6.8.3; tested in a Windows 10 QEMU VM |

Windows VM playback was tested with software rendering. Physical Windows GPU acceleration and controller input remain unverified. Other distributions, Windows versions, and architectures are not yet validated.

## Downloads and installation

**[Download Preview 3 for Linux or Windows](https://github.com/dcenhance/cloudstream-pc/releases/tag/v0.1.0-preview.3)**. These are experimental desktop builds, not an official CloudStream release. Choose a runnable package below, not GitHub's automatic source-code ZIP.

| Download | Installation and requirements |
| --- | --- |
| **Windows x64 Setup EXE** | Run the installer. Installs for the current user, adds shortcuts and an uninstaller; application updates do not replace your profile. |
| **Windows x64 ZIP** | Extract the **entire ZIP** to a writable folder, then run `cloudstream.exe`. Do not run it inside the ZIP or copy just the EXE. Qt, Java, media libraries and Visual C++ runtime DLLs are included. |
| **Linux amd64 DEB** | Ubuntu 24.04 baseline. Install the downloaded file with `sudo apt install ./cloudstream-pc_0.1.0.preview.3_amd64.deb` so dependencies are resolved. |
| **Linux x86_64 RPM** | Fedora/Nobara-family package; install with `sudo dnf install ./cloudstream-pc-0.1.0-0.preview.3.x86_64.rpm`. Full installation on a clean RPM-based desktop is not yet validated. |
| **Linux x86_64 system-runtime AppImage** | Make executable, then run. **Not self-contained:** requires system Qt, mpv, SDL2, Java and FFmpeg. See the [exact dependencies and FUSE-less launch option](packaging/linux/README.md). |

Linux packages require **glibc 2.39 or newer and Qt 6.4.2 or newer**. They are not universal Linux binaries. Windows packages target Windows 10 x64 or later; testing uses a Windows 10 VM with software rendering, not a physical GPU/controller certification.

The Windows installer is **unsigned**. Windows may display an unknown-publisher warning; verify the release and its SHA-256 checksums before deciding whether to run it. Do not disable antivirus or system protections to install the application.

The release includes `SHA256SUMS`, application source archives, and separate third-party source/support archives. Source/support TAR files are for rebuilding and license compliance, **not extra files required to launch the app**. Keep the shipped `licenses/` and Java legal directories. LGPL shared libraries remain replaceable; debugging modifications to those libraries is permitted by their licenses.

First launch: add an extension repository you trust in **Extensions**, install providers, then select one on Home. No repositories or provider accounts are bundled. Uninstalling removes application files, not your saved profile.

## Features

### Preview 3: provider Home fix

The provider host now writes its JSON protocol explicitly as UTF-8. This fixes empty AniWorld Home on Windows caused by Java's legacy system code page rejecting non-ASCII titles in Qt's JSON parser. The 20-second request deadline is unchanged.

Home now distinguishes genuinely empty results from invalid JSON, failed requests, crashes, startup failures and timeouts. Failed refreshes preserve cached Home; partial provider results remain usable. Regression checks include a real JVM subprocess using Windows-1252 and a Windows 10 GUI comparison: the old runtime rendered no sections, while the corrected runtime completed 62/62 sections at capture time. Live provider content can change.

These are focused Home fixes, not a new certification of playback or hardware support. Graceful shutdown of the FUSE-mounted AppImage remains unverified; prior Wayland startup checks required forced cleanup. SHA-256 checksums establish file integrity, not trusted publisher authentication.

- Rounded dark desktop UI with Home, Search, Library, Downloads, Settings, and Extensions.
- Provider-scoped Home search, global search filters, and an in-app provider picker.
- Poster-based Continue Watching and history management.
- Embedded video playback, seeking, subtitles, audio tracks, and fullscreen controls.
- Resumable direct downloads and FFmpeg-backed adaptive downloads.
- Extension repository management and a separate JVM provider host.
- Keyboard and SDL2 controller navigation.

Clean profiles contain no preinstalled extension repositories. Add only repositories you trust.

## Build

### Linux

Install a C++17 toolchain, Qt 6 development packages (Widgets, Network, Concurrent, OpenGL, SVG and Test), qmake6, SDL2 and libmpv development packages, Java 17, and FFmpeg. The retained upstream Gradle configuration also configures Android modules and may require an Android SDK.

From the repository root, with `JAVA_HOME` set to your Java 17 installation:

```sh
./linux-native/build.sh
./linux-native/test.sh
./linux-native/build/cloudstream-linux
```

The build script's fallback Java path is distribution-specific; set `JAVA_HOME` explicitly on other systems. Linux uses XDG storage rather than writing profiles into the source tree.

### Windows

Use an x64 MSVC developer shell with Qt 6 MSVC tools and Java 17 on PATH. Supply the libmpv and SDL2 headers, import libraries, and runtime DLLs through the environment variables documented in [the Windows build guide](linux-native/WINDOWS_PORT.md).

```powershell
.\linux-native\build-windows.ps1
```

The script builds `cloudstream.exe` and stages Qt, the provider host, and Java in `dist-windows`. It accepts a prebuilt provider-host distribution, additional runtime DLLs, FFmpeg, and an optional modern Mesa software-rendering fallback. Review third-party licenses and runtime dependencies before redistributing this directory.

## Project layout

| Directory | Purpose |
| --- | --- |
| `linux-native/` | Shared Linux **and Windows** Qt application, tests, and packaging scripts |
| `provider-host/` | JVM provider execution and Android compatibility shims |
| `library/` | Upstream CloudStream APIs and extractors, with desktop changes |
| `app/` | Retained upstream Android module and original assets |
| `linux/` | Earlier JVM desktop prototype; not the primary desktop client |

## Compatibility and security

Extensions execute third-party code. Running them in a separate process improves application isolation but **is not a security sandbox**. Do not install untrusted extensions.

Android `.cs3` compatibility depends on conversion and the APIs used by each extension. DRM, arbitrary Android activities, WebView challenge solving, and Android account integrations are not supported. Provider availability and upstream websites can change independently of this application.

Adaptive-download pause/resume on Windows uses dynamically resolved NT process APIs; errors are reported rather than treated as a successful pause. Those APIs are not a guaranteed public Win32 contract.

Report issues with your OS, build method, reproduction steps, and sanitized logs. Never post credentials, private viewing history, or signed media URLs.

## License

Distributed under the upstream **GNU GPL version 3**; see [LICENSE](LICENSE). Existing copyright and attribution notices are retained. Third-party dependencies retain their own licenses.

This source snapshot is based on upstream revision [`efc1915fd6c075699490581f01371145ee0af9eb`](https://github.com/recloudstream/cloudstream/commit/efc1915fd6c075699490581f01371145ee0af9eb), with the Linux/Windows frontend, provider-host compatibility work, and desktop integration changes. CloudStream branding and original assets originate from the upstream project.