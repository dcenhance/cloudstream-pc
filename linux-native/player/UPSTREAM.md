# CloudStream player: upstream reference and desktop adaptation

## Pinned, live-verified upstream

Official repository: https://github.com/recloudstream/cloudstream

The upstream `master` endpoint was resolved live to **81dbdf4b4483ee72566f108ac9cde998a79e519e**. The latest stable release endpoint returned **v4.8.0**, published **2026-07-10T00:16:24Z**. The styling reference is the newer pinned master source, not an assumed appearance from a release screenshot.

Source references at that revision:

- [player_custom_layout.xml](https://github.com/recloudstream/cloudstream/blob/81dbdf4b4483ee72566f108ac9cde998a79e519e/app/src/main/res/layout/player_custom_layout.xml): centered title/information, 70dp back/transport targets, quarter/half/three-quarter transport positions, overlaid seek-time row, scrollable bottom actions.
- [FullScreenPlayer.kt](https://github.com/recloudstream/cloudstream/blob/81dbdf4b4483ee72566f108ac9cde998a79e519e/app/src/main/java/com/lagradost/cloudstream3/ui/player/FullScreenPlayer.kt): `animateLayoutChanges`, 100ms alpha transitions and Android-specific 200ms translations; subtitle/metadata/episode visibility.
- [colors.xml](https://github.com/recloudstream/cloudstream/blob/81dbdf4b4483ee72566f108ac9cde998a79e519e/app/src/main/res/values/colors.xml): primary `#3d50fa`, black overlay `#66000000`, primary black `#111111`, text `#e9eaee`, videoProgress `#66B5B5B5`.
- [styles.xml](https://github.com/recloudstream/cloudstream/blob/81dbdf4b4483ee72566f108ac9cde998a79e519e/app/src/main/res/values/styles.xml): transparent `VideoButton`, light `WhiteButton` Apply and dark `BlackButton` Cancel.
- [player_select_tracks.xml](https://github.com/recloudstream/cloudstream/blob/81dbdf4b4483ee72566f108ac9cde998a79e519e/app/src/main/res/layout/player_select_tracks.xml), [player_select_source_and_subs.xml](https://github.com/recloudstream/cloudstream/blob/81dbdf4b4483ee72566f108ac9cde998a79e519e/app/src/main/res/layout/player_select_source_and_subs.xml), and [speed_dialog.xml](https://github.com/recloudstream/cloudstream/blob/81dbdf4b4483ee72566f108ac9cde998a79e519e/app/src/main/res/layout/speed_dialog.xml): dark selection panels, paired columns, Apply/Cancel, centered speed value, white slider, +/- steps and 0.25/1/1.25/1.5/2 presets, 0.1–2 range.

`upstream/` contains the original Android vectors and geometry-preserving SVG conversions used by Qt. `import-upstream-icons.py` reproducibly fetches that exact revision and converts the paths (no hand-redrawn replacements). Rewind mirrors the original forward icon and draws the configured seek interval at its center. The GPL-3.0 license downloaded from that revision is retained as `upstream/LICENSE`; it matches this repository's root license. Attribution remains with the CloudStream contributors. The importer is a development tool, not a runtime network dependency.

## Window lifecycle

Single-window playback is now a managed page of `appSurfaces`, a stack around the **whole** application content. The existing sidebar/pages remain together as the other page. Switching to playback hides navigation without reserving its width, and does not reparent or replace the running GL renderer. Back restores the prior navigation page/focus; status notifications cannot reserve a footer during playback. Separate-window mode remains separate.

Native monitor fullscreen is distinct from filling the app content. It targets `player->window()`, not the embedded `Qt::Widget` dialog. The original embedded path called `showFullScreen()` on a non-window, leaving the actual host window unchanged. The original separate path unconditionally restored normal mode and therefore lost maximization.

Save the host's preceding window state, and restore it on toggle/Back/reject. Qt retains the normal geometry. Do **not** combine state restoration with a second `restoreGeometry()`: on real Wayland this reproduced a second-entry failure where the fullscreen flag was set but the window retained its normal dimensions. Native full-screen dimensions and repeated cycles are tested, not just flags. F, F11, double-click and Escape use this same lifecycle.

## Visual/input changes

- Original upstream play/pause, seek, back, lock, fit, speed, source, track and fullscreen vectors, with original transparent transport styling instead of the old black pill buttons.
- Real 40% black scrim over continuously rendered video; transparent top/center/bottom containers. The old ancestor stylesheet `background:#000` incorrectly propagated black backgrounds to child containers. A pixel regression covers this. The scrim is explicitly painted because the OpenGL overlay's no-system-background attribute suppresses stylesheet-only background painting.
- Thin primary-colored timeline, primary thumb, 30px slider interaction area, white time labels; large independent center controls remain centered as the window changes size. Bottom actions scroll at narrow widths instead of widening the video window.
- Source, audio/subtitle and speed panels stay inside the player, without a new native window. Their shade resizes with playback. Controller navigation is scoped to the panel; player shortcuts are disabled except Escape while it is open. Escape cancels speed changes too, rather than only the Cancel button doing so.
- The 100ms chrome fade animates the overlay only, never the QOpenGLWidget, and removes its graphics effect when finished. Existing pause/loading/auto-hide logic remains. Lock stays reachable through the scroll-area ancestors.

## Deliberate differences / not claimed

This is a close Qt adaptation of the supported player UI, **not pixel-identical Android rendering or complete Android feature parity**. Android Material ripples, Roboto/sp scaling and 200ms per-view translation are not duplicated; Qt uses desktop font metrics, hover/focus feedback and the 100ms fade. The bottom row retains real desktop volume and native-fullscreen controls (the phone layout normally hides its fullscreen icon). Source selection remains separate from the combined audio/subtitle track chooser rather than adopting Android's video/audio versus source/subtitle split. Selection rows use Qt selection feedback.

The player currently receives a media title and resolved sources/tracks, not the full Android player metadata/episode generator. Optional movie-logo/synopsis scrim, episode drawer/next-episode/skip-op, torrent header, cast/PiP, rotation and touch brightness gestures are therefore not fabricated or exposed as dummy controls. Existing subtitle rendering stays with libmpv. Fit can retain aspect-ratio letterboxing: that is video content, not reserved app-navigation space.

Linux is exercised using real libmpv playback on isolated KWin Wayland and XCB/Xwayland. Windows uses the same sources and resources; each published Windows package must pass the native hosted player/fullscreen suites and EXE/ZIP lifecycle gates recorded in that release's notes. Hosted software rendering is not physical GPU, audio-device or controller certification. Packaging preserves the Preview 5 updater and derives every package identity from `linux-native/VERSION.txt`.

## Repeatable verification

- `linux-native/tests/test_mpv_player_widget.cpp`: generated video/audio/subtitle playback, tracks, seeking, fallback, preferences, auto-hide, upstream transport dimensions, native embedded fullscreen, fade, scoped panel keys and cancellation, reachable lock.
- `linux-native/tests/test_single_window_surfaces.cpp`: production main-window embedding path, generated video decoded/rendered/advanced by libmpv, embedded/separate × normal/maximized native fullscreen cycles, screen-sized geometry, F/F11/Escape/double-click/Back, XCB position-and-size restoration, panel captures, navigation restoration and scrim pixels.
- `linux-native/test.sh`: complete Qt suite; run with an isolated profile and real compositor, not offscreen for the renderer/fullscreen gates.

Local execution logs, upstream API responses, generated fixture and before/after/menu/restoration PNGs are under `.local-evidence/player/` (intentionally not release assets). Some native Qt accessibility and libmpv OpenGL INVALID_ENUM warnings are present on this host; successful decoded frames, visual captures and assertions are reported separately from those warnings. An initial updater build-identity test failure was a stale mixed-version object build; cleaning that test build fixed it without changing updater source or weakening the assertion.
