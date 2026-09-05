# CloudStream Linux

The Linux frontend is a native Qt 6 desktop application in `linux-native/`.
It uses KDE/Linux-native window behavior and a coherent dark media-center UI.

## Run

```bash
~/.local/bin/cloudstream-linux
```

The launcher builds `linux-native/cloudstream-linux` automatically if needed.
You can also find **CloudStream Linux** in the application menu or on the
Desktop.

## Features

- Home dashboard with direct media URL playback through `mpv`
- Provider selector with real CloudStream Home sections and poster carousels
- Provider-backed Search poster grid and media-type filters
- In-app title details, episode lists, link extraction, and playback
- Extension repository browser with live `repo.json`/plugin-list loading
- Short-link resolution for Android-compatible repository formats
- Verified `.cs3` extension downloads with SHA-256 checksums
- Downloads folder access
- Persistent playback and folder settings
- Dark responsive Qt 6 desktop styling
- Scrollable Settings layout without compressed or clipped cards

The app stores preferences using the native Qt settings store. CloudStream
still does not ship media sources by default; repositories and their content
remain user-controlled.

## Build manually

```bash
cd linux-native
qmake6 cloudstream-linux.pro CONFIG+=release
make -j2
```

The older Kotlin/JVM frontend remains in `linux/` as a migration reference for
platform-neutral CloudStream code. The Android UI cannot be reused unchanged
on Linux because it depends on Android framework classes.
