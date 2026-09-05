# CloudStream PC

CloudStream for **Linux and Windows**, maintained by [dcenhance](https://github.com/dcenhance). A shared Qt 6 desktop interface with an out-of-process JVM extension runtime and embedded libmpv playback.

An independent desktop adaptation of [recloudstream/cloudstream](https://github.com/recloudstream/cloudstream), not an official upstream desktop release.

## Contents

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

This repository contains the source for **both versions**. Public binary releases are pending packaging and third-party redistribution review; no installer or universal Linux package is promised yet.

## Features

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