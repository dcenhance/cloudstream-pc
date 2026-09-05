# Preview 2 release provenance

This document describes the final release arrangement. `windows-licenses/REDISTRIBUTION-AUDIT.md` and `source-closure/` retain historical investigations of the initial static Windows bundle. Their original shader/static-media findings must not be mistaken for the identity of the replacement runtime.

## What is distributed

- The application is a GPLv3 CloudStream derivative. GitHub's tagged source and the explicit release source archives include the desktop code, provider-host, library, build files and packaging scripts.
- Linux packages use distribution-managed Qt, libmpv, SDL2, Java and FFmpeg. They include the shared provider-host JARs and legal/source material. The AppImage is explicitly a **system-runtime** package, not a self-contained bundle.
- Windows keeps shared Qt 6.8.3, Temurin Java 17, the provider-host, SDL2, software OpenGL libraries and app-local release VC143 CRT DLLs. License texts, notices and component metadata accompany the runtime.
- Windows libmpv/FFmpeg and their imported dependencies are replaced with the recorded MSYS2 UCRT64 snapshot. The original shinchiro static libmpv and Gyan static FFmpeg distributions are **not** the release media runtime. See `msys2-runtime/` for binary/source checks and reconstruction commands.
- The unknown initial shader DLL set is not shipped. The tested public Microsoft DXIL validator is retained, with its notices; other original shader DLLs are omitted. Testing establishes the software-rendering path, not every GPU/backend combination.

## Source/support assets

The runnable ZIP/installer and Linux packages do not require users to extract source-support TAR files. Those files provide source, build metadata, applicable patches, copyright notices and license material for developers and recipients exercising source rights.

- `CloudStream-PC-0.1.0-preview.2-source.*`: application source from the published release commit.
- `CloudStream-PC-0.1.0-preview.2-Common-sources.tar`: Qt, Java, shared JVM dependency sources/notices, software-renderer/SDL source material and follow-up attribution evidence.
- `CloudStream-Windows-MSYS2-sources.tar`: exact media-runtime source packages and their package build metadata.
- `CloudStream-Windows-MSYS2-supplementary.tar`: additional static/header inputs and lockfile-checksummed Rust crates.
- `CloudStream-Windows-MSYS2-metadata.tar`: source validation, package provenance and runtime inventories.

The MSYS2 checks compare repository hashes, binary `.BUILDINFO` against source PKGBUILDs, direct-source checksums and imported DLL symbols. They do not claim independent package-signature authentication or a bit-for-bit rebuild of every upstream binary. Do not describe this evidence as a legal certification.

Qt/LGPL shared libraries remain dynamically linked and replaceable. Their source/license terms permit modification and reverse engineering for debugging those modifications. Microsoft files retain Microsoft's terms; Java's Classpath/assembly exceptions and component-specific notices are retained. No blanket GPL relabeling of dependencies is intended.

## Application-owned JVM outputs

`provider-host-4.8.0.jar` and `library-jvm-1.0.1.jar` are application outputs rather than Maven releases. `jvm-build-provenance.json` binds their hashes to the retained source files and original desktop build workspace comparison. The source tree and Gradle build scripts are included with the release. The d2j-external Maven source JAR is manifest-only; the follow-up under `windows-licenses/jvm-shaded-followup/` records real upstream inputs/notices instead of treating the empty artifact as source coverage.

## Verification boundary

Release notes describe the actual final package tests. Earlier static-runtime tests do not validate the MSYS2 replacement. Physical Windows GPU acceleration/controller input, every Linux distribution, DRM and Android WebView/account integrations are not certified by these builds.
