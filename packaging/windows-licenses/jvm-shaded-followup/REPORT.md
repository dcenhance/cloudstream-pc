# Focused shaded-JVM and Mesa notice follow-up

## d2j-external 2.4.38: provenance and notices resolved

Git tag `2.4.38` at https://github.com/ThexXTURBOXx/dex2jar resolves to `ecdd1b5074e8f8ad1ade959b2a65b4babfe1b3d2` (git ls-remote). Downloaded the immutable commit archive from `https://codeload.github.com/ThexXTURBOXx/dex2jar/tar.gz/ecdd1b5074e8f8ad1ade959b2a65b4babfe1b3d2`.

Artifact: `/data/CloudStream-releases/0.1.0-preview.2/windows-source-support/sources/jvm-followup/dex2jar-2.4.38-ecdd1b5074e8f8ad1ade959b2a65b4babfe1b3d2.tar.gz`
SHA256: `10c4de49da568e784bb97aa819f3d9864fb7e172b7631b10ac603506404d7e1e`.

The tagged `d2j-external/build.gradle` shades `../libs/*.jar`. These are `asm-9.10.1-mctle.jar` and `dx-30.0.3.jar`. Compared the actual runtime JAR, read-only, against these exact vendored binaries: all 609 Android class files and all 38 non-module ASM class files are byte-identical. ASM's module-info.class is omitted by shading. These account for all 647 runtime classes; no differing class bytes. Detailed hashes and evidence are in `evidence.json`.

The Maven `d2j-external-2.4.38-sources.jar` contains only `META-INF/` and its manifest: **it is not corresponding source code**. The new dex2jar archive contains build scripts and vendored binary dependencies, not complete source for the patched ASM/Android binaries. The separately collected upstream ASM 9.10.1 source JAR supplies current copyright/license text; it is not claimed to be the exact source of the `mctle` modifications. These Apache/BSD licenses do not impose a corresponding-source delivery obligation for binary redistribution.

### Copy into final runtime licenses before packaging

- `dex2jar/LICENSE.txt` and `dex2jar/NOTICE.txt`: exact tagged Apache-2.0 license and project attribution.
- `ASM-9.10.1-copyright-and-BSD.txt`: full BSD-3-Clause text and `Copyright (c) 2000-2011 INRIA, France Telecom`, extracted from the matching upstream ASM source version. All Java source copyright lines were checked and use this attribution. Vendored manifest explicitly identifies BSD-3-Clause.
- `android-dalvik-NOTICE.txt`: Android upstream NOTICE including Apache-2.0 and Android Open Source Project copyright. Retrieved from Android 11 (`android-11.0.0_r1`), not asserted as an exact build-source mapping for dx 30.0.3.
- `android-dx-Main.java.header.txt`: upstream dx Apache copyright header.
- Retain `dex2jar/dex-tools/open-source-license.txt` as the exact upstream bundled-dependency notice. It includes historical ASM attribution plus an ANTLR section; that ANTLR section is not evidence ANTLR occurs in d2j-external.

The `vendored/` manifests and build.gradle are provenance evidence, not additional licenses. No runtime files were changed.

## Mesa-related narrowly scoped missing permissive texts

Reviewed existing source-support `licenses/downloads-mesa-notices.json`, upstream LLVM/zlib/zstd licenses, and Mesa buildinfo. LLVM 22.1.8 full Apache-with-exceptions and legacy NCSA text, zlib 1.3.2 copyright/license, and zstd 1.5.7 BSD copyright/license are already collected: no duplicate replacements needed.

Concrete additional LLVM Support notices absent from existing source-support license text collection (checked for Henry Spencer and Yann Collet):

- `LLVM-22.1.8-Support-COPYRIGHT.regex.txt`: actual LLVM-tagged regex copyright/terms, not covered by a generic LLVM license label.
- `LLVM-22.1.8-Support-xxhash.h.header.txt`: LLVM-tagged xxHash BSD-2-Clause copyright/terms, Yann Collet 2012-2016.

These are conservative notices for third-party code in the stated LLVM dependency; this bounded inspection does not prove individual LLVM object inclusion in Mesa.

The prior SPIRV-Tools `v2026.3.1/LICENSE` fetch failed. Mesa buildinfo additionally identifies LunarG Vulkan SDK **1.4.357.0**. Successfully collected actual SPIRV-Tools and SPIRV-Headers LICENSE texts from the `vulkan-sdk-1.4.357.0` upstream tags, including SPIRV-Headers' component copyright/terms, plus the Tools optimizer copyright header (Google 2016). Include these supplemental files. **SDK-tag notice provenance is explicit; it does not establish that Tools 2026.3.1 equals that tag.** An attempted Tools NOTICE fetch returned 404; no NOTICE was fabricated.

## Remaining work / limitations

Parent must copy the identified notice texts to the final shipped license tree, correct any inventory/report claiming the empty Maven JAR is source, then regenerate final package manifests/archives. This task deliberately does not touch active runtime directories or make commits.

No corresponding-source blocker is asserted for these permissive dependencies. Exact SDK-tag versus Tools 2026.3.1 source identity remains unverified; upstream patched ASM/Android build-source provenance is likewise not claimed. Full Mesa third-party object-level notice completeness is outside this bounded follow-up. JSON records actual origins, file sizes and SHA256 values; all recorded notice hashes were read back and verified.
