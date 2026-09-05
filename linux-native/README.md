# CloudStream Linux (Qt)

This is the primary native Linux client. It uses Qt 6 for the desktop UI and runs compatible CloudStream provider JARs through the separate JVM `provider-host` process.

## Build and test

Requirements: Qt 6 Widgets/Network/Concurrent development files, qmake 6, a C++17 compiler, JDK 17, SDL2, and `mpv` for playback.

```sh
./build.sh
./test.sh
```

The binary is written to `build/cloudstream-linux`.

## Smoke tests

```sh
QT_QPA_PLATFORM=offscreen ./build/cloudstream-linux \
  --smoke-test --expect-platform=offscreen
./build/cloudstream-linux --version
```

Smoke mode skips repository/provider networking, presents one Qt event-loop cycle, prints `SMOKE_TEST_OK`, and exits.

## Controller navigation

SDL-compatible controllers are detected and hot-plugged automatically. D-pad and the left stick move focus; A activates; B goes back; Start toggles playback; triggers page-scroll; and the shoulder buttons switch main pages. In the player, shoulders seek, Y toggles fullscreen, and X toggles mute. Held directions repeat after a short delay, and controller input is ignored while CloudStream is not the active application.

## Staged install

The qmake install target stages the GUI, provider host, desktop entry, icon, and AppStream metadata into one relocatable prefix:

```sh
stage="$PWD/stage"
make -C build install INSTALL_ROOT="$stage"
```

The canonical layout is:

```text
/usr/bin/cloudstream-linux
/usr/libexec/cloudstream/provider-host/bin/cloudstream-provider-host
/usr/libexec/cloudstream/provider-host/lib/*.jar
/usr/share/applications/io.github.recloudstream.cloudstream.desktop
/usr/share/icons/hicolor/scalable/apps/io.github.recloudstream.cloudstream.svg
/usr/share/metainfo/io.github.recloudstream.cloudstream.metainfo.xml
```

At runtime, the GUI resolves the provider host from `CLOUDSTREAM_PROVIDER_HOST`, then from the installed relative `libexec` path, then from the repository development build. This keeps packaged builds independent of any developer home directory.

## Extension compatibility

Linux can execute extensions that publish compatible JVM JARs. Android `.cs3` archives contain DEX bytecode and cannot be executed directly by a desktop JVM. Downloaded artifacts are hash-checked, and providers run outside the Qt process.
