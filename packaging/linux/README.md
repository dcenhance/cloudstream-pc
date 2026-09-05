# Linux preview packages

Version: 0.1.0-preview.4, x86-64 only. Native GUI compiled inside Ubuntu 24.04
with its Qt 6.4.2 toolchain, not the host's Nobara Qt 6.11 toolchain.
The shared source uses Qt 6.4's `setTransferTimeout(20000)` milliseconds API,
equivalent to the newer chrono overload's 20 seconds. No private application
source patch is applied by the packager.

These are **system-runtime packages**, including the AppImage. They include the
native executable and the JVM provider-host distribution, but deliberately do
not bundle Qt, libmpv, SDL, FFmpeg, Java, glibc or graphics drivers. The AppImage
is an executable type-2 SquashFS image, not a source archive or a fully
self-contained runtime. It needs the same runtime dependencies as the DEB.

Supported build/test baseline: Ubuntu 24.04 amd64 (glibc 2.39). The declared
minimum is glibc 2.39, Qt 6.4.2, libmpv.so.2, SDL2 2.30, Java 17 and FFmpeg 6.
Do not claim Ubuntu 22.04 compatibility. The RPM uses the same Ubuntu-built
ELF and declares the corresponding SONAME/version dependencies; Fedora/Nobara
may satisfy these, but a clean RPM-distro install is not established by an
extracted-package smoke test.

Ubuntu 24.04 runtime installation for the AppImage:

```sh
sudo apt install libqt6widgets6 libqt6network6 libqt6openglwidgets6 libqt6concurrent6 libqt6svg6 libmpv2 libsdl2-2.0-0 qt6-qpa-plugins qt6-wayland qt6-image-formats-plugins openjdk-17-jre-headless ffmpeg ca-certificates
chmod +x CloudStream-PC-0.1.0-preview.4-x86_64-system-runtime.AppImage
./CloudStream-PC-0.1.0-preview.4-x86_64-system-runtime.AppImage
# FUSE-less alternative supported by the upstream type-2 runtime:
./CloudStream-PC-0.1.0-preview.4-x86_64-system-runtime.AppImage --appimage-extract-and-run
```

The DEB/RPM install `cloudstream-pc`, desktop integration and provider-host
under `/usr/libexec/cloudstream/provider-host`. The AppImage launcher sets an
app-relative provider-host override, never a developer-home path. Runtime
state uses the application's existing XDG paths; no user state is packaged.

Graceful shutdown of the FUSE-mounted AppImage remains unverified; the prior
Wayland startup check required forced cleanup. Extract-and-run startup is a
separate bounded smoke check, not shutdown or playback certification.

## Build

`container-build.sh` runs inside Ubuntu 24.04 with `/src` read-only repository
and `/out` writable release directory. `ubuntu.sources` is a signed Ubuntu
mirror configuration; use an HTTPS mirror and valid CA store if the default
HTTP mirror stalls. The actual build used rootless Podman with host networking.
`package.py` stages the built ELF, the unchanged, audit-matched Preview 3 Java 17 JVM distribution,
licenses, corresponding project source and the shared JVM license audit. It
then invokes dpkg-deb, rpmbuild and mksquashfs in the build container.

The type-2 runtime is from AppImage/type2-runtime, continuous x86_64, with its
exact binary SHA256 and release/source metadata retained beside the output.

## Licensing and redistribution

CloudStream is GPL-3.0. All packages carry LICENSE, corresponding application
source/build scripts, unmodified JARs, JAR notices/POM provenance, license
texts and matching GPL/MPL dependency source. The JVM components retain their
own licenses. The AppImage runtime carries its own upstream MIT notice/source
reference. System runtime libraries are not redistributed in these packages;
obtain their source/license notices from your distribution.

Before public redistribution, review the shared JVM audit for any unresolved
provenance or notice issue. These packaging smoke tests are not a legal audit,
and launch survival is not proof of streaming, DRM, hardware video decoding,
controller support or a clean graphical distro installation.
