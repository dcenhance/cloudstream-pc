# In-app application updates

Open **Settings → Updates and backup → CloudStream PC updates**. This is the PC application's updater, separate from extension updates.

- **Check for updates** asynchronously reads only `https://api.github.com/repos/dcenhance/cloudstream-pc/releases/latest`.
- Current/latest compatible versions, plain-text release notes, progress, cancellation and actionable errors stay inside the application.
- **Download update** asks for confirmation and downloads the exact platform/package asset. It never installs on download completion.
- The optional startup check is **off by default**, persisted as `updates/automaticCheck`, and runs after a short startup delay. It only reports availability in the existing status UI. It never downloads, opens a browser, installs, or interrupts playback with a dialog.
- GitHub's `latest` endpoint is the selected channel: releases flagged as prereleases on GitHub are not normally returned, even though version tags such as `v0.1.0-preview.4` can belong to a release published on this channel. Numeric prerelease identifiers are compared numerically, and stable versions outrank their prereleases. Same/older versions never enable downloading.

## Installation support and limits

| Running package | Confirmed action after verification |
| --- | --- |
| Linux AppImage | Re-check SHA-256; stage beside the original `APPIMAGE`, preserve execution permissions, create `.backup`, atomically replace the original. Restart requires a **separate explicit confirmation**. A read-only path, symlink, extracted/copied executable, mount path or existing backup is rejected rather than overwritten. Restore by closing the app and renaming the retained `.backup` back to the original name. |
| Windows installed Setup | Re-check SHA-256 and executable signature prefix, then launch only the matching official `Windows-x64-Setup.exe`, with **no shell, no silent flags, and no elevation flags**, and gracefully quit CloudStream. The interactive per-user installer updates the default `%LOCALAPPDATA%\Programs\CloudStream PC` location and preserves the separate profile. Non-default locations are not automatically handled. Native Windows tests validate the direct handoff mechanism with a harmless executable and separately validate the actual installer lifecycle; Linux tests are not Windows evidence. |
| Windows portable ZIP | Verified download and folder reveal only. Close CloudStream and extract into a **new folder**. No running-file overwrite or ZIP execution; the Qt-standard-path profile is separate. |
| Linux DEB / RPM | Verified download and folder reveal only. Open/install with the distribution's package manager yourself. CloudStream does not request root, invoke sudo, collect credentials, or claim the system package was installed. |
| Source/development, missing/mismatched package identity | Verified download only (AppImage on Linux x86_64, ZIP on Windows x64); never execute or install it from the development build. Other architectures have no compatible download. |

AppImage and Windows Setup also offer **Show verified package for manual installation** as an explicit fallback when in-place replacement or the supported installer location is unavailable.

Downloads are stored in a private `cloudstream-update-*` temporary directory and atomically finalized only after verification. Cancelling or closing before handoff removes temporary data. Accepting the final handoff retains the verified file and shows its exact path; retained files may be removed manually after installation (the OS may also clear temporary storage on reboot). Application settings, provider libraries and the profile are never updater write targets. Download/verification is asynchronous; AppImage staging and pre-handoff re-verification run off the GUI thread. Navigation preserves an active download; app shutdown cancels it. Closing a page during AppImage staging waits for that bounded file operation, rather than abandoning a partially staged replacement.

## Verification and trust

The transport reuses `CloudStreamRequest::metadata` (TLS validation, HTTP/1.1 policy and a 20-second inactivity timeout) with a PC-specific User-Agent and a **stricter manual redirect policy**. Only the exact repository's GitHub download paths and the official `release-assets.githubusercontent.com` / `objects.githubusercontent.com` release-asset CDN paths are permitted. No HTTP downgrade, credentials in URLs, alternate ports, arbitrary domains or arbitrary assets. Redirects are limited to five; checks have a 60-second total deadline and downloads a 30-minute deadline. Metadata is capped at 2 MiB, displayed notes at 64 KiB, asset lists at 100 entries, and packages at 1 GiB. Filename, release tag, size and digest are validated before downloading.

Every package must have GitHub's `sha256:` asset digest. **Missing/invalid digest, size discrepancy or SHA-256 mismatch fails closed.** There is deliberately no unverified fallback or independently downloaded checksum-file trust shortcut. GitHub asset digests authenticate consistency with the official repository's metadata, not an independent publisher signing key. A compromised publisher account remains inside this trust boundary. Release notes are inert plain text, not executable HTML or auto-loaded remote content.

## Building and publishing compatible releases

`linux-native/VERSION.txt` is the single hand-edited semantic release version. Change it **before compiling**, not just before renaming archives. Qmake embeds it into the application, sets the native numeric version and writes `cloudstream-version.txt` into the build output. The application reports it with `--version` and in Settings.

The Windows build script copies the version stamp into the runtime. Linux's container build exports the paired stamp. Packaging scripts reject a different stamp/version rather than relabel a previous executable. They put `cloudstream-build.json` beside the executable with the exact version and one package kind (`windows-setup`, `windows-zip`, `appimage`, `deb`, `rpm`). The installer writes its installed identity without changing the portable runtime identity. A missing, mismatched or cross-platform marker is treated as a development build.

Keep release asset names consistent with `updates/ReleasePolicy.h`; all five existing Preview 4 names are covered. Upload new assets normally so GitHub computes a SHA-256 digest. Do not replace package-format semantics beneath an existing filename. Older builds without the updater need one manual installation of a release containing this feature. Preview 5 is the first release containing this updater. Automatic startup checks remain opt-in.

## Tests and diagnostics

- `linux-native/test.sh` includes `tests/updater.pro` and the integrated Settings tests, in addition to the existing native suite. Run GUI tests on an isolated compositor or X server.
- Updater tests use explicitly labelled local/mock fixtures and exercise numeric versions, all five package names, incompatible/missing Windows assets, malformed metadata, hash/size failure, error/rate-limit responses, redirect rejection/limits, cancellation/retry/destruction, explicit consent, and atomic AppImage replacement with a preserved backup. Fixture package bytes are never executed.
- `python3 -m unittest discover -s packaging -p 'test_*identity.py'` checks package identity; `test_windows_installer.py` checks NSIS quoting. NSIS syntax can be compiled on Linux using non-executable fixture files, without launching an emulator.
- Build `tests/updater-probe.pro` to run `updater_probe` for a live check with the actual compiled version. `updater_probe --verify-download` explicitly simulates an old version **only in this diagnostic**, downloads the live AppImage to an isolated temporary directory, verifies it, prints its size/digest, deletes it on exit, and never installs/executes it.
- The `updaterSettingsRenderAndLiveCheck` native GUI test enables a real GitHub check only when `CLOUDSTREAM_UPDATER_LIVE=1`; `CLOUDSTREAM_TEST_EVIDENCE` selects the screenshot output directory.
