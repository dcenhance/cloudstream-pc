# MSYS2 UCRT64 media runtime staging

`stage.py` copies an existing Windows runtime to a separate candidate, replaces only the media runtime and stages recursive PE imports from the **saved** UCRT64 repository file database. It preserves the MSVC application/Qt/JVM. It never executes package build recipes. Requires Python 3.11+, curl, bsdtar, zstd and GNU objdump on Linux.

```sh
python3 packaging/msys2-runtime/stage.py \
  --original /data/CloudStream-releases/0.1.0-preview.2/windows-final-runtime \
  --runtime /data/CloudStream-releases/0.1.0-preview.2/windows-msys2-runtime \
  --support /data/CloudStream-releases/0.1.0-preview.2/windows-msys2-source-support \
  --seed /data/CloudStream-releases/0.1.0-preview.2/windows-source-support/source-closure/msys2-candidate
```

The support directory is a cache/lock: retain `ucrt64.db`, `ucrt64.files` and binary packages to reproduce the same snapshot. Delete neither database independently. Use a new support directory to resolve a newer snapshot. Never point `--runtime` at the original. Package downloads are checked against repository SHA-256; source PKGBUILD SHA-256 is checked against the exact binary `.BUILDINFO`, including split-package `pkgbase` mapping. Exact build environment `.BUILDINFO`, `.PKGINFO`, `.MTREE`, `.SRCINFO`, patches and full source archives are retained. Source member hashes and declared SHA-256 coverage are reported, including explicit SKIP values; package signatures are not independently authenticated.

Only imported DLL packages and necessary runtime data packages are selected, not all build dependencies. Windows system/API-set imports are explicitly allowlisted; unknown imports fail. This import-level evidence is not a universal assertion about statically linked/header-only code, optional dynamically loaded modules, Rust crates or the unrelated Qt/JVM/Mesa source audit. DLL source bundles preserve distributor sources and instructions; inspect recorded exceptions before publication.

## Finish and verify

After staging, run these from the repository (substitute the same runtime/support paths):

```sh
python3 packaging/msys2-runtime/finalize.py --runtime "$RUNTIME" --support "$SUPPORT" \
  --shader-runtime "$ORIGINAL_SHADER_RUNTIME" --dxil /tmp/cloudstream-public-dxil.dll
python3 packaging/msys2-runtime/supplement.py --support "$SUPPORT" --runtime "$RUNTIME"
python3 packaging/msys2-runtime/verify_archive.py --runtime "$RUNTIME" --support "$SUPPORT" --archives
```

`finalize.py` additionally needs Git, bubblewrap and Fedora's `trust` command/layout. It generates the install-time-empty MSYS2 CA bundles using only that package's Mozilla trust policy, not the host's CA additions. `CloudStream.cmd` sets app-relative TLS/fontconfig paths; TLS verification is never disabled. The original shader DLL trio is retained unless `--minimal-shaders` is explicitly selected after Windows testing. `--dxil` supplies the separately audited public Microsoft DXIL validator instead of the original unknown build. Use `verify_archive.py`, not the older finalize `--archives` shortcut, for the complete supplementary archive.

`supplement.py` collects exact declared header inputs and shaderc's statically linked glslang/SPIRV-Tools packages from consumer `.BUILDINFO`, plus all registry crates in retained Cargo.lock files (including platform/test-only extras). Every crate download is checked against its lock checksum; any Git dependency stops the collector. Licenses are copied verbatim into the runtime as well as remaining inside source archives.

`verify_archive.py` verifies app-local PE imported **symbols**, not just DLL names, including the original MSVC GUI's libmpv requirements. Git-backed source checksums use `git archive --format=tar REF` as makepkg does. Source-file hashes and Git archive hashes together close the recorded direct-source hash checks; SKIP signatures remain explicitly unauthenticated.

Outputs: `CloudStream-Windows-MSYS2-runtime.zip`, separate `sources.tar`, `supplementary.tar`, `metadata.tar`, and outer SHA256SUMS. ZIP contents are CRC-tested. Source support remains separate from runnable payloads. Runtime manifests include every actual file, including shader DLLs and legal/data files. The binary-package cache is retained locally for snapshot reproduction; it is not part of the source-only archive.

The runtime is staged flat next to `cloudstream.exe`. FFmpeg's DLLs are therefore available to both its executable and libmpv without relying on PATH. Packaged fontconfig and TLS data are also retained. Do not disable TLS verification to compensate for missing trust data.

The Preview 2 application sets these packaged TLS/font paths at Windows startup when the corresponding files exist, preserving explicit environment overrides. Launch `cloudstream.exe` directly; the legacy `CloudStream.cmd` is not required by the installer or normal use.

A Windows clean-PATH VM test is mandatory before release: application startup, libmpv initialization, actual local/HTTPS/HLS playback, seeking/audio/subtitles, FFmpeg HTTPS remux, hardware/software rendering, and shutdown. The original static binaries' prior tests do not validate this replacement. No application source changes, commits or publication are performed by these scripts.
