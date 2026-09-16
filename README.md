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
| Linux x86-64 | Preview 6 player/fullscreen changes tested on Nobara/KDE Wayland and XCB; release packages use an Ubuntu 24.04 / Qt 6.4.2 baseline |
| Windows x86-64 | Preview 6 is built with MSVC 2022 / Qt 6.8.3 on hosted Windows Server 2022; publication is gated on native player/fullscreen suites and EXE/ZIP lifecycle checks reported in the release notes |

Earlier Windows 10 VM playback was tested with software rendering. Preview 6 uses a native hosted Windows build for the shipping artifacts, then stages that exact published package in the user's disposable Windows 10 VirtualBox guest for manual fullscreen testing. Neither environment establishes physical Windows GPU acceleration or controller support. Other distributions, Windows versions, and architectures are not yet validated.

## Downloads and installation

**[Download Preview 6 for Linux and Windows](https://github.com/dcenhance/cloudstream-pc/releases/tag/v0.1.0-preview.6)**. All five runnable formats are provided together: Windows Setup EXE/portable ZIP and Linux AppImage/DEB/RPM, with application and dependency source/support archives and `SHA256SUMS`. [Previous Preview 5 downloads](https://github.com/dcenhance/cloudstream-pc/releases/tag/v0.1.0-preview.5) remain unchanged. These are experimental desktop builds, not an official CloudStream release. Choose a runnable package below, not GitHub's automatic source-code ZIP.

| Download | Installation and requirements |
| --- | --- |
| **Windows x64 Setup EXE** | Run the installer. Installs for the current user, adds shortcuts and an uninstaller; application updates do not replace your profile. |
| **Windows x64 ZIP** | Extract the **entire ZIP** to a writable folder, then run `cloudstream.exe`. Do not run it inside the ZIP or copy just the EXE. Qt, Java, media libraries and Visual C++ runtime DLLs are included. |
| **Linux amd64 DEB** | Ubuntu 24.04 baseline. Install the downloaded file with `sudo apt install ./cloudstream-pc_0.1.0.preview.6_amd64.deb` so dependencies are resolved. |
| **Linux x86_64 RPM** | Fedora/Nobara-family package; install with `sudo dnf install ./cloudstream-pc-0.1.0-0.preview.6.x86_64.rpm`. Full installation on a clean RPM-based desktop is not yet validated. |
| **Linux x86_64 system-runtime AppImage** | Make executable, then run. **Not self-contained:** requires system Qt, mpv, SDL2, Java and FFmpeg. See the [exact dependencies and FUSE-less launch option](packaging/linux/README.md). |

Linux packages require **glibc 2.39 or newer and Qt 6.4.2 or newer**. They are not universal Linux binaries. Windows packages target Windows 10 x64 or later; hosted Windows Server 2022 and a disposable Windows 10 VirtualBox guest are software-rendered verification environments, not physical GPU/controller certification.

The Windows installer is **unsigned**. Windows may display an unknown-publisher warning; verify the release and its SHA-256 checksums before deciding whether to run it. Do not disable antivirus or system protections to install the application.

The release includes `SHA256SUMS`, application source archives, and separate third-party source/support archives. Source/support TAR files are for rebuilding and license compliance, **not extra files required to launch the app**. Keep the shipped `licenses/` and Java legal directories. LGPL shared libraries remain replaceable; debugging modifications to those libraries is permitted by their licenses.

First launch: add an extension repository you trust in **Extensions**, install providers, then select one on Home. No repositories or provider accounts are bundled. Uninstalling removes application files, not your saved profile.

## Features

### Preview 6: full-area player and native fullscreen fixes

Single-window playback now occupies the complete app content area instead of leaving the navigation sidebar or status footer reserved. Back restores the prior page and focus. Separate-window playback remains available.

Embedded and separate players now send native fullscreen to the actual top-level host while keeping libmpv's OpenGL renderer embedded. Leaving fullscreen, closing the player, or pressing Back restores the preceding normal or maximized state; repeated F, F11, double-click and Escape cycles are covered with decoded-frame and compositor-geometry assertions.

The player chrome is closely adapted from official [`recloudstream/cloudstream@81dbdf4b4483ee72566f108ac9cde998a79e519e`](https://github.com/recloudstream/cloudstream/commit/81dbdf4b4483ee72566f108ac9cde998a79e519e); the latest stable upstream release resolved at implementation time was **v4.8.0**. Original GPL vectors and attribution are retained. The desktop player keeps real volume, native-fullscreen, hover/focus and Qt sizing behavior. Android-only episode generation, torrent/cast/PiP, rotation, touch brightness, account/WebView/DRM integrations and pixel-identical Material rendering are not claimed.

Preview 5's opt-in, consent-gated updater remains present. Exact Preview 6 Windows/Linux suite totals, package lifecycle results and any hosted audio/framebuffer limitations are reported in the release notes; successful software-rendered tests do not claim physical GPU, audio-device or controller coverage.

### Preview 5: application updater

Settings → **Updates and backup → CloudStream PC updates** checks the official GitHub latest release, displays compatible versions and notes, and downloads only the matching package with mandatory GitHub SHA-256 verification. Startup checks are optional and off by default; downloading and installing always require user choice.

Writable original AppImages support atomic replacement with a retained `.backup` and separate restart confirmation. The default per-user Windows Setup installation supports an interactive installer handoff. Windows ZIP, Linux DEB/RPM, and development builds offer verified downloads for **manual installation**, not automatic installation. Non-default Windows install locations must also update manually. See [the updater contract and recovery instructions](linux-native/UPDATER.md). Checksums verify consistency with repository metadata, not an independent trusted publisher identity; Windows installers remain unsigned.


Native Windows [run 34281709565](https://github.com/dcenhance/cloudstream-pc/actions/runs/34281709565) rebuilt the Preview 5 executable with MSVC/Qt 6.8.3. Updater (12), Settings (10), single-window surfaces (8), Home process results (10), details presentation (4), and player commands (7) passed. A harmless native executable verified the same argument-free, shell-free handoff mechanism used by the updater, including paths with spaces/Unicode/metacharacters; this is not a real older-to-newer unattended update test. ZIP CRC/payload hashes, package identities, clean-path launches, packaged Java/provider-host and FFmpeg, per-user install/version registry and safe uninstall preserving a user-added file passed. Audited dependency bytes are unchanged.

**Media limits remain:** Windows media tests have 10 passes / 1 audio-track-selection failure on the audio-device-less hosted runner. Ubuntu 24.04/Qt 6.4.2 full tests have 242 passes / 1 framebuffer-color failure across 32 suites with private null audio. The initial audio-less Ubuntu run failed audio-track selection instead; a separate null-audio media run passed 11 cases, so the framebuffer issue remains intermittent, not fixed. Updater/Settings suites pass on both platforms. The DEB was installed with dependencies and launched in a fresh Ubuntu userspace container; this is not a clean graphical-desktop or physical-hardware certification. No local VirtualBox, QEMU or Wine was used.

### Preview 4: single-window navigation overlap fix (retained)

Navigating between pages now dismisses embedded details/player dialogs instead of allowing them to overlap Search or other pages. Closing details disconnects UI completion delivery before cancelling its provider process; an older dialog cannot clear the newer dialog’s resize pointer. Separate dialog windows remain open as intended.

Linux and native Windows regression coverage exercise loading/loaded opacity, page navigation, cancellation while loading, newer-dialog resizing, and embedded-player versus separate-window behavior. [Windows recovery run 33995672025](https://github.com/dcenhance/cloudstream-pc/actions/runs/33995672025) built the exact tagged Preview 4 application commit `b2d1f9709d289980af2566108334c11e20b268f1`: the executable is newly compiled, not renamed from Preview 3. Audited dependencies and provider-host JARs remain byte-identical; their Common/MSYS2 source-support archives are reused unchanged. Later workflow/NSIS tooling fixes do not move the release tag or change application source.

Required Windows suites passed: single-window surfaces (7), Home process results (10), details presentation (4), and player commands (7). ZIP CRC/member hashes, extracted clean-path launch, packaged Java/provider-host and FFmpeg startup, silent installation, installed payload hashes/launch, version registration and safe uninstall preserving a user-added file passed. The native test fixture replaces only the POSIX shell helper; assertions are unchanged. **The Windows media suite has 10 passes and 1 failure** at audio-track selection on the audio-device-less hosted runner, with hardware/OpenGL warnings; no full media-suite pass or audio/GPU fix is claimed. No local VirtualBox, QEMU or Wine was started.

The Ubuntu software-rendered container full test run has one unresolved framebuffer-readback failure (228 passes / 1 failure); the overlap suite and separate isolated-Wayland full run pass. See [Linux verification limits](packaging/linux/README.md). Package startup checks do not certify playback.

### Preview 3: provider Home fix (retained)

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