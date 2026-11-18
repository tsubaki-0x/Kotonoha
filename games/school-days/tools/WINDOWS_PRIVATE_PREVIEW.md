# Local-only Windows release preview — eebeee10

The independent experimental DEV (d2de823f) and hybrid (eebeee10) worktrees
and compiled binaries remain unmodified. The new staging tool
stage_windows_clean_local.py prepares an explicitly PRIVATE QA preview from
the existing Windows Release DEV or DIST using a positive file allowlist.
No game, installer, GPK, save, build, Git merge or push is executed by this tool.

## Pinned inputs

- Hybrid worktree with its Release output folders, pinned submodules and
  third-party license files.
- The previously installed unmodified Noto Sans JP v2.004 file, 5,733,060
  bytes, SHA-256 7C8597677E9FAC0F54D7848AD18BC6A708DFB5BAA4EBF4BD91E66EFCCA313BF3.
  The OpenType name table attributes Adobe 2014–2021 (RFN Source) and declares
  SIL Open Font License 1.1. Full canonical text is bundled with the source
  staging tool in licensing/SIL-OFL-1.1.txt and included in the preview.
  The stage script refuses a different font hash, including Windows fonts.
- The existing x64-windows-static-md vcpkg installed share/ directory. It
  must provide the 13 expected port copyright notices; no network download
  is attempted.

Command usage (supply verified absolute paths, destination must not exist):

    python -B games/school-days/tools/stage_windows_clean_local.py
      --profile dev
      --source-release PATH_TO_HYBRID_BUILD/dev-clean/outputs/Release/SchoolDays
      --source-repo PATH_TO_HYBRID_WORKTREE
      --vcpkg-share PATH_TO_VCPKG_INSTALLED/x64-windows-static-md/share
      --font PATH_TO_ORIGINAL_LICENSED_NOTO/NotoSansJP-Regular.ttf
      --destination PATH_TO_NEW_LOCAL_STAGE
      --private-local-preview

For DIST replace dev with dist and the source Release path with
dist-standalone/outputs/Release/SchoolDays. The example is explanatory:
enter the entire command on one terminal line or use proper PowerShell
continuation syntax. A failed stage retains INCOMPLETE-DO-NOT-USE.txt and
does not delete or overwrite an existing destination.

## Content and rights boundary

The preview contains exactly SchoolDays.exe and four dependency DLLs,
42 GLSL shader files plus source/license notices, the unmodified OFL font,
styles.ass, the existing school-days-hq.ktnroute, an explicit private-only
README, MIT/project/submodule notices and all 13 available vcpkg notices.
Each stage includes manifest.json, manifest.sha256 and a deterministic
sorted/timestamp-normalized ZIP. It NEVER includes retail GPK, saves,
personal settings, debug PDB/LIB, translations or game media.

SPLASH.PNG, icon.ico, launcher HD art and Display Options PNG/CMAP are
intentionally EXCLUDED until redistribution rights have been demonstrated.
The 1.7 MB KTRF routing graph is retail-derived metadata; separate
redistribution rights have not been established. Both folder and ZIP
are therefore **PRIVATE LOCAL QA ONLY — DO NOT PUBLISH OR SHARE**.
The Noto font may be bundled unchanged with its copyright and full OFL 1.1
but its glyph set does not cover all simplified Chinese characters.

Never execute the binary inside the finalized staging folder: DEV may
create dev-startup.log alongside the executable. Instead copy or unpack
the package into a fresh temporary QA runtime and pass isolated
--documents, --config and working directory. Supply user-owned Packs
through --school-days, without copying them into any public package.
Do not execute the retail EXE/installer.

The five binaries import external Windows x64 MSVC runtime libraries
including MSVCP140 and VCRUNTIME140; the clean-machine VC++ runtime gate is
not tested. Windows FFmpeg and some other dependencies link statically:
LGPL source/relink obligations need independent review before release.
Anime4K SOURCE.txt cites only three of 39 shaders, so provenance of the
remainder needs review. Game-specific engine/asset/route rights and legal
permissions for images/translations also require review before publishing.

Windows physical retail/visual QA remains with the user: SetSELECT/Feel,
AUTO, hover, Skip barrier, Back, Replay, NextChoice, Save/Load, atomic data,
seven LxH epilogues, translations, icon/launcher and caption colors.
Four-frame SDL dummy/software smoke does not establish full parity. An
unstable RX 580 alone cannot establish an engine graphics regression.
Android still needs SDK/NDK native builds and physical device QA.

## Focused tests

    python -B -m unittest discover -s games/school-days/tools -p test_stage_windows_clean_local.py -v

Tests cover GPK/save/secret rejection, unapproved font checksum, duplicate
destination refusal and deterministic ZIP generation. The final stage/ZIP
manifest validation and DEV/DIST synthetic smokes are recorded in the
ignored local homologation report, not in the repository.

Neither DEV/hybrid experimental candidate, official local branch nor
origin/upstream remote branch is promoted here. Future fast-forward
promotion requires explicit user permission; push requires separate approval.
