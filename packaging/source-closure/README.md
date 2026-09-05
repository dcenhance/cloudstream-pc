# Windows GPL static-source closure — concrete follow-up

## Outcome

**The two tested static distributions are not yet source-complete.** This follow-up recovered actual build records and matching source snapshots, rather than treating an unidentified build history as a universal redistribution prohibition. No runtime files were changed, no target tests were run, and no commits were made. Qt, Microsoft CRT, Mesa and Java are outside this focused finding; it does not add blockers for them or resolve their separate audits.

Evidence is in `/data/CloudStream-releases/0.1.0-preview.2/windows-source-support/source-closure/`. This directory was added after the existing source-support tar was generated: the parent must regenerate its release archive and outer inventory if incorporating it. `SHA256SUMS.json` inventories this follow-up; `verification.json` records executed checks. `msys2-candidate/` contains evaluation binary packages as well as source packages, **not staged runtime replacements**.

## Tested shinchiro libmpv

* Exact build: [run 33697571182](https://github.com/shinchiro/mpv-winbuild-cmake/actions/runs/33697571182), build head `cd1edc11dc6887a50f705717619d879f5a93a488`, release `release20260903`, mpv `69e63f425a`, FFmpeg `9fc8c785e`.
* Authenticated `gh api .../actions/runs/33697571182/logs` successfully returned the actual public job logs. Full ZIP and extracted **x86_64**, not x86_64_v3, job are retained.
* The job restored source and x86_64 build caches from run `33573448041`, updated repositories, rebuilt changed components and packaged the requested mpv revision. Container digest: `sha256:6156ca503061914e1e73c3efa7276d14f5d45c78b3b8534c46e60294500beb66`.
* Importantly, per-component logs artifact `9873346961` **expired 2026-09-04T00:41:27Z**. An authenticated download attempt returned **HTTP 410 “Artifact has expired”**. The main Actions log survives, but does not print all cached repository HEADs. `mpv-artifacts.json` and `expired-log-response.txt` preserve this concrete failure.
* The build recipe writes exact upstream HEADs to `build_x86_64/packages/<name>-prefix/src/<name>-stamp/HEAD` before applying patches (`cmake/custom_steps.cmake`, `write-head`). Those files, plus cached sources, are the precise missing recovery target. The public cache-list response contains an x86_64_v3 cache from this run, not its exact x86_64 source/build caches; a cache listing is not source delivery. No upstream workflow was modified to expose caches.

### Matching inputs now collected

`mpv-source-evidence.json` indexes the build recipes and records full commits, URLs, archive SHA-256 and retained legal files. It is **not** an assertion that every recipe is linked into libmpv. It includes unused/conditional build recipes, distinguished from collected inputs.

Twenty source inputs were fetched successfully using explicit recipe pins/digests or the actual job's branch-update/rebuild evidence:

* Changed repositories: opus `8f39f97`, harfbuzz `2093bb59`, libarchive `2f81aa8`, highway `17abbca`, SDL2 `b90ac950`, SPIRV-Tools `0db1457`, glslang `3b7ce57`, ngtcp2 `b5de53a`. GitHub commit responses resolve each abbreviation to its full SHA in the manifest. The stored recipe determines which branch is used; arbitrary fetched branch updates are **not** treated as compiled revisions.
* Recipe-pinned: ANGLE headers, AviSynth+ headers, libpsl, libsixel, OpenSSL, VapourSynth, XZ.
* Versioned/digest-pinned: libiconv **1.18**, libopenmpt **0.7.12**, LZO **2.10**, Xvid **1.3.7**, Opus model archive **8a07d57c4fce6fb30f23b3e0d264004e04f1d7b421f5392ef61543d021a439af**. Every declared archive digest for these downloads was checked, including LZO's recipe SHA-1; SHA-256 receipts are also retained.

The full exact build-recipe tree and patches were extracted from the already-collected head archive into `mpv-recipe/`. Source archives retain copyright-bearing files; separate COPYING/LICENSE/COPYRIGHT/NOTICE/AUTHORS files are copied verbatim to `mpv-notices/`. This does not claim that submodule or Rust crate vendoring is complete.

### Codec revisions recovered from the actual tested binaries

`binary-codec-versions.json` binds the evidence to SHA-256 of both deployed binaries. These are codec version-string identifications, not claims that there were no distributor patches.

| Component | Tested binary | Embedded version | Matching upstream commit |
|---|---|---|---|
| x264 | both | `r3223 0480cb0` | `0480cb05fa188d37ae87e8f4fd8f1aea3711f7ee` |
| x265 | gyan ffmpeg.exe | `4.3+6-9ddc216` | `9ddc216defd1fa85535cb91456a9f6c09a2c3abc` |
| x265 | shinchiro libmpv-2.dll | `4.3+22-98eec40` | `98eec4057fc20850b59ed292b3f13ddeefc2f93f` |

All three source archives and their actual legal texts were acquired. x264 was fetched from the GitHub `mirror/x264` mirror at the matching commit; x265 from `Multicorewareinc/x265`. Full upstream commit responses and download hashes are in `codec-source-evidence.json` and `commit-metadata/`.

### What remains for this exact libmpv

The unresolved part is **actual static dependency source closure**, not the availability of mpv/FFmpeg themselves. Missing exact cached revisions include Rubber Band, libdvdnav/libdvdread/libdvdcss, libass and its font-stack inputs, plus other enabled codec/network/rendering dependencies and transitive inputs. The surviving log also gives update candidates for AOM, fontconfig and libssh, but they were not collected in this bounded pass. Floating references in the archived recipes do not supply those missing snapshots. Subrandr's Rust dependency sources and any git submodules must be included where used. Build patches already present in the head archive should be applied to the recovered *upstream* snapshots, not to guessed current branches.

Request from shinchiro: the x86_64 run's `src_packages`, per-package stamp `HEAD` files, source-download/configure/build logs, relevant Cargo.lock/vendor material, and toolchain/build-input records. This is a precise retrieval request; no contact was sent. If those cannot be recovered, a new pinned build or a different source-backed distribution is required for the release payload to have an assembled corresponding-source set.

## Tested gyan FFmpeg 9.0.1 essentials

`gyan-release-9.0.1.json` is the publisher's exact release metadata. Its Source link identifies FFmpeg **`bf1b838f2a`**; its assets are binary builds, not dependency source bundles. The previously collected FFmpeg 9.0.1 source remains relevant. `gyan-builds.html` preserves the publisher's build description; `gyan-tree-main.json` shows the support repository contains README/FUNDING, not this build's dependency lock or recipe. A failed initial `master` branch lookup is retained separately and was corrected to `main`.

This pass recovered matching x264 and x265 upstream sources above. **It did not recover the 9.0.1 essentials build's complete external-library revision/patch/configuration record.** The deployed build configuration explicitly enables GPL/version3/static plus GnuTLS, GMP, libass, fontconfig/freetype/fribidi/harfbuzz, cairo, SRT/SSH/ZMQ, codecs, vid.stab, VMAF, Rubber Band and more. Their exact source inputs cannot be replaced by a link to FFmpeg's own repository or by the current state of media-autobuild_suite. A diagnostic string mentioning “GnuTLS 3.6.16 logging” is not used as version proof.

Request from gyan: the exact 9.0.1 essentials source/download set and dependency versions/commits, build scripts/settings, patches, and transitive sources. No assertion is made that the distributor cannot supply them; the inspected public release and repository did not provide them.

## Practical replacement: MSYS2 UCRT64 shared packages

Two real candidate binaries **and their distributor-provided source-only packages** were downloaded:

| Candidate | Source-only package | Binary SHA-256 |
|---|---|---|
| `mingw-w64-ucrt-x86_64-mpv-0.41.0-6-any.pkg.tar.zst` | `mingw-w64-mpv-0.41.0-6.src.tar.zst` | `75565784cc87c6ba21b2f86b665631da2b41b005114f33716c284b11b345348e` |
| `mingw-w64-ucrt-x86_64-ffmpeg-9.0.1-3-any.pkg.tar.zst` | `mingw-w64-ffmpeg-9.0.1-3.src.tar.zst` | `e443030a329e08685cc894fe4f9edd393a1a6a8272a774f66708b612954baa4c` |

Source URLs are the exact `https://mirror.msys2.org/mingw/sources/<source-only package>` paths; saved package pages and receipts supply all URLs/hashes. Source archives actually contain the upstream source, PKGBUILD and .SRCINFO; FFmpeg additionally contains its applied path-relocation patch and pathtools C/header. The verification script compares each source PKGBUILD's SHA-256 to the corresponding binary's **.BUILDINFO `pkgbuild_sha256sum`**, and verifies all non-skipped PKGBUILD input hashes against archived source members. Binary hashes match the saved publisher pages. Package signatures were not independently verified in this pass.

The `.BUILDINFO` files retain exact installed build-package versions, not merely a list of dependency names. PE inspection confirms `libmpv-2.dll` imports shared avcodec/avformat/libass/Rubber Band/etc., and the MSYS2 ffmpeg.exe imports the versioned FFmpeg DLLs. Import reports and .PKGINFO/.BUILDINFO are in `msys2-inspection/`.

**This is a concrete source-backed replacement route, not an already complete replacement runtime.** Collect the recursive non-system DLL dependency packages from the same UCRT64 repository snapshot, plus each exact source-only package, license notices and any relevant static/header-only build inputs identified by the recipes/.BUILDINFO. Validate the resulting PE closure. Do not blindly bundle MSYS2's entire environment: `winpty` is used by the mpv console wrapper; the application uses libmpv and must be assessed on that path. Shared packages make provenance/source matching tractable but do not erase source or attribution obligations.

### Rebuild and testing decision

* No rebuild of these MSYS2 libraries is inherently needed: their binaries and matching source/build recipes already exist.
* CloudStream's rebuild is **conditional**, not automatically required by changing distributors: if it uses the compatible libmpv C ABI and no newer unavailable symbols/options, a DLL replacement can work without recompiling the application. Compare exported symbols/API versions and application use before deciding. This pass did not perform that compatibility check.
* **A fresh Windows test is required** after any replacement. Different mpv version/configuration, FFmpeg shared-library layout, shader stack, Lua choice and dependency DLLs mean the previous static-build test is not transferable. At minimum test startup on clean PATH, actual libmpv initialization/playback, local and HTTPS/HLS media, seeking/audio/subtitles, supported GPU and software-render paths, FFmpeg download/remux operations, and shutdown. Keep the tested original untouched until this replacement candidate passes.

## Reproduction

Run the scripts in `packaging/source-closure/` from a host with Python, authenticated `gh`, `zstd`, and GNU `objdump`:

1. `python3 collect_evidence.py`
2. `python3 collect_sources.py`
3. `python3 inspect_candidates.py`
4. `python3 verify_collection.py`

They write only the dedicated source-closure evidence tree (plus copied scripts there), not the deployed runtime. Network endpoints such as cache lists and package pages are current-state evidence; archived run IDs, full source commits and versioned package URLs are the durable anchors. This is technical source/provenance evidence, not a legal-clearance certificate.
