#!/usr/bin/env python3
"""Verify optional hybrid Android source contracts without running Android tools.

Read-only: only selected Gradle, Java, CMake, ProGuard and workflow source text
is inspected. No game archives, translations, saves, SDK, NDK or APKs are read.
If the separate Android/ project does not exist (upstream-only branch), skip.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
from typing import Dict, List, Mapping, Optional, Tuple


UPSTREAM = "buildsystems/android"
CUSTOM = "Android"
ACTIVITY = "app/src/main/java/org/kotonoha/android/GameActivity.java"
PROGUARD = "app/proguard-rules.pro"
MANIFEST = "app/src/main/AndroidManifest.xml"
DEV_FIXTURE = (
    "app/src/schoolDaysDev/java/org/kotonoha/android/"
    "SchoolDaysDevSaveFixtures.java"
)
SHARED_COPY = "buildsystems/cmake/KotonohaApp.cmake"
WORKFLOW = ".github/workflows/android.yml"
REQUIRED = (
    "CMakeLists.txt",
    "lib/CMakeLists.txt",
    "buildsystems/cmake/KotonohaAndroid.cmake",
    "buildsystems/cmake/KotonohaOptions.cmake",
    SHARED_COPY,
    WORKFLOW,
    f"{UPSTREAM}/build.gradle",
    f"{UPSTREAM}/settings.gradle",
    f"{UPSTREAM}/app/build.gradle",
    f"{UPSTREAM}/{MANIFEST}",
    f"{UPSTREAM}/{ACTIVITY}",
    f"{UPSTREAM}/{PROGUARD}",
    f"{CUSTOM}/build.gradle",
    f"{CUSTOM}/settings.gradle",
    f"{CUSTOM}/CMakeLists.txt",
    f"{CUSTOM}/app/build.gradle",
    f"{CUSTOM}/{MANIFEST}",
    f"{CUSTOM}/{ACTIVITY}",
    f"{CUSTOM}/{PROGUARD}",
    f"{CUSTOM}/{DEV_FIXTURE}",
)
EXPECTED_LIBRARIES = ["c++_shared", "SDL3", "SDL3_ttf", "imgui",
                      "kotonoha_engine"]
JNI_METHODS = (
    "getKotonohaResourceDirectory",
    "getKotonohaPackageIdentifier",
    "getKotonohaGameAssetDirectory",
    "getKotonohaDataDirectory",
    "getKotonohaDocumentsDirectory",
    "getKotonohaDisplayModes",
    "getKotonohaCurrentDisplayModeId",
    "setKotonohaPreferredDisplayMode",
)
TRANSLATION_INCLUDES = {
    "jsons/*.json", "scripts/en-lxh.GPK", "scripts/pt-BR-lxh.GPK",
    "scripts/es-lxh.GPK", "scripts/zh-CN-lxh.GPK",
    "scripts/Script - Brazilian.GPK", "scripts/Script - Spanish.GPK",
    "scripts/Script - chinese-compatible.GPK",
}
DEV_SAVES = {f"SaveFile{i:03d}.DAT" for i in range(7)} | {"GlobalFlag.DAT"}


def strip_comments(source: str) -> str:
    """Mask comments, preserving strings and newlines for precise locations."""
    out: List[str] = []
    state = "code"
    quote = ""
    i = 0
    while i < len(source):
        c = source[i]
        n = source[i + 1] if i + 1 < len(source) else ""
        if state == "line":
            out.append("\n" if c == "\n" else " ")
            if c == "\n":
                state = "code"
        elif state == "block":
            if c == "*" and n == "/":
                out.extend("  ")
                i += 1
                state = "code"
            else:
                out.append("\n" if c == "\n" else " ")
        elif state == "string":
            out.append(c)
            if c == "\\" and n:
                out.append(n)
                i += 1
            elif c == quote:
                state = "code"
        elif c in ("'", '"'):
            out.append(c)
            quote = c
            state = "string"
        elif c == "/" and n == "/":
            out.extend("  ")
            i += 1
            state = "line"
        elif c == "/" and n == "*":
            out.extend("  ")
            i += 1
            state = "block"
        elif c == "#":
            out.append(" ")
            state = "line"
        else:
            out.append(c)
        i += 1
    return "".join(out)


def braced(source: str, header: str) -> Optional[str]:
    """Extract a balanced Groovy/Java {...} block matching a header."""
    match = re.search(header, source, re.S)
    if not match:
        return None
    first = source.rfind("{", match.start(), match.end())
    if first < 0:
        return None
    depth = 1
    quote = ""
    i = first + 1
    while i < len(source):
        c = source[i]
        if quote:
            if c == "\\":
                i += 2
                continue
            if c == quote:
                quote = ""
        elif c in ("'", '"'):
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return source[first + 1:i]
        i += 1
    return None


def included_patterns(closure: str) -> List[str]:
    result: List[str] = []
    for match in re.finditer(
        r"\binclude\s+(.+?)(?=\binto\s+|\})", closure, re.S
    ):
        result.extend(re.findall(r"'([^']+)'", match.group(1)))
    return result


class Checker:
    def __init__(
        self, root: Path, overrides: Optional[Mapping[str, str]] = None,
        custom_exists: Optional[bool] = None,
    ) -> None:
        self.root = root
        self.overrides = overrides or {}
        self.custom_exists = custom_exists
        self.files: Dict[str, str] = {}
        self.problems: List[str] = []

    def fail(self, path: str, reason: str, anchor: str = "") -> None:
        text = self.files.get(path, "")
        pos = text.find(anchor) if anchor else -1
        line = text.count("\n", 0, pos) + 1 if pos >= 0 else 1
        self.problems.append(f"{path}:{line}: {reason}")

    def require(self, path: str, pattern: str, reason: str,
                anchor: str = "") -> None:
        if not re.search(pattern, self.files[path], re.S | re.M):
            self.fail(path, reason, anchor)

    def project(self, project: str, upstream: bool) -> None:
        gradle = f"{project}/app/build.gradle"
        root_gradle = f"{project}/build.gradle"
        settings = f"{project}/settings.gradle"
        manifest = f"{project}/{MANIFEST}"
        parent = (
            r"rootProject\.projectDir\.parentFile\.parentFile"
            if upstream else r"rootProject\.projectDir\.parentFile(?!\.)"
        )
        cmake = "../../../CMakeLists.txt" if upstream else "../CMakeLists.txt"
        self.require(
            gradle, rf"\bdef\s+engineRoot\s*=\s*{parent}\b",
            "incorrect independent engineRoot", "engineRoot",
        )
        self.require(
            gradle, rf"\bpath\s+['\"]{re.escape(cmake)}['\"]",
            "Gradle CMake path crosses project roots", "externalNativeBuild",
        )
        self.require(
            settings, r"^\s*include\s+['\"]:app['\"]",
            "Gradle project needs its own :app module", "include",
        )
        self.require(
            settings, r"\brootProject\.name\s*=\s*['\"]KotonohaAndroid['\"]",
            "missing independent Gradle root", "rootProject",
        )
        self.require(
            root_gradle, r"\bid\s+['\"]com\.android\.application['\"]"
            r"\s+version\s+['\"][^'\"]+['\"]\s+apply\s+false",
            "independent Android application plugin declaration missing",
            "com.android.application",
        )
        self.require(
            gradle, r"\bnamespace\s+['\"]org\.kotonoha\.android['\"]",
            "expected JNI/manifest namespace missing", "namespace",
        )
        self.require(
            gradle,
            r"-DVCPKG_OVERLAY_PORTS=\${new\s+File\(engineRoot,\s*"
            r"['\"]vcpkg-overlays/ports['\"]\)\}",
            "vcpkg overlay must point to vcpkg-overlays/ports",
            "VCPKG_OVERLAY_PORTS",
        )
        self.require(
            gradle, r"\bproguardFiles\b[^}]*['\"]proguard-rules\.pro['\"]",
            "Release must reference its own ProGuard rules", "proguardFiles",
        )
        self.require(
            gradle, r"\bproguardFiles\b[\s\S]*?sdlRoot[^\n]*proguard-rules\.pro",
            "Release must also reference SDL JNI rules", "proguardFiles",
        )
        for activity in ("BootstrapActivity", "GameActivity"):
            self.require(
                manifest, rf'android:name="\.{activity}"',
                f"manifest missing {activity}", activity,
            )
            self.require(
                f"{project}/{PROGUARD}",
                rf"^\s*-keep\s+class\s+org\.kotonoha\.android\.{activity}"
                r"\s*\{\s*\*\s*;\s*\}",
                f"R8 must keep all {activity} JNI methods", activity,
            )
        java = f"{project}/{ACTIVITY}"
        get_libraries = braced(
            self.files[java],
            r"\bprotected\s+String\s*\[\s*\]\s+getLibraries\s*\(\s*\)\s*\{",
        )
        body = re.search(
            r"return\s+new\s+String\s*\[\s*\]\s*\{([^}]+)\}\s*;",
            get_libraries or "", re.S,
        )
        if not body or re.findall(r'"([^"]+)"', body.group(1)) != (
            EXPECTED_LIBRARIES
        ) or not re.search(
            r",\s*BuildConfig\.NATIVE_TARGET\s*$", body.group(1)
        ):
            self.fail(
                java, "getLibraries must load libimgui.so and the selected "
                "game target in SDL dependency order", "getLibraries",
            )
        for method in JNI_METHODS:
            self.require(
                java, rf"\bpublic\s+(?:String|int\[\]|int|boolean)\s+{method}\s*\(",
                f"JNI callback {method} missing", method,
            )
        self.require(
            gradle, r"\bdef\s+includeGameAssets\s*=\s*"
            r"providers\.gradleProperty\(['\"]bundleGameAssets['\"]\)",
            "game-asset inclusion must honor -PbundleGameAssets",
            "bundleGameAssets",
        )
        for pattern in (
            r"tasks\.register\(['\"]stage\${taskSuffix}Assets['\"],\s*Sync\)",
            r"variant\.mergeAssetsProvider\.configure\s*\{\s*dependsOn\s+assets",
            r"android\.sourceSets\[variant\.name\]\.assets\.srcDir\s+staged",
        ):
            self.require(
                gradle, pattern, "Gradle must be the owner of Android "
                "asset staging and merge dependencies", "stage",
            )

    def selectors_and_variants(self) -> None:
        generic = f"{UPSTREAM}/app/build.gradle"
        custom = f"{CUSTOM}/app/build.gradle"
        for path in (generic, custom):
            self.require(
                path, r"\bschoolDays\s*:\s*\[\s*'school-days'\s*,\s*"
                r"'SchoolDays'\s*,\s*'me\.hirameki\.kotonoha\.schooldays'",
                "School Days target/applicationId must retain base identity",
                "schoolDays",
            )
            self.require(
                path, r"\bapplicationId\s+game\[2\]",
                "flavor must select the matrix applicationId", "applicationId",
            )
        self.require(
            generic, r"if\s*\(\s*flavor\s*==\s*'schoolDays'\s*\)"
            r"\s*\{\s*applicationIdSuffix\s+'\.upstream'\s*\}",
            "generic School Days must use a separate .upstream identity",
            "applicationIdSuffix",
        )
        if re.search(r"applicationIdSuffix\s+'\.upstream'", self.files[custom]):
            self.fail(custom, "custom DEV/DIST cannot use .upstream identity")
        self.require(
            generic, r"-DKOTONOHA_BUILD_GAME=\${game\[0\]\}",
            "generic Gradle must select canonical game variable",
            "KOTONOHA_BUILD_GAME",
        )
        self.require(
            custom, r"-DKOTONOHA_ANDROID_GAME=\${game\[0\]\}",
            "custom Gradle must pass its game selector", "KOTONOHA_ANDROID_GAME",
        )
        cmake = f"{CUSTOM}/CMakeLists.txt"
        self.require(
            cmake, r"set\s*\(\s*KOTONOHA_BUILD_GAME\s+"
            r"\"\${KOTONOHA_ANDROID_GAME}\"\s+CACHE\s+STRING\s+"
            r"\"[^\"]+\"\s+FORCE\s*\)",
            "custom CMake must bridge its selector to KOTONOHA_BUILD_GAME",
            "KOTONOHA_BUILD_GAME",
        )
        self.require(
            cmake, r"add_subdirectory\s*\(\s*\.\.\s+kotonoha\s*\)",
            "custom CMake must include the canonical engine root",
            "add_subdirectory",
        )
        bridge = self.files[cmake].find("set(KOTONOHA_BUILD_GAME")
        subdir = self.files[cmake].find("add_subdirectory(.. kotonoha)")
        if bridge >= 0 and subdir >= 0 and bridge >= subdir:
            self.fail(cmake, "selector bridge must precede engine configuration",
                      "set(KOTONOHA_BUILD_GAME")
        self.require(
            "buildsystems/cmake/KotonohaOptions.cmake",
            r"if\s*\(\s*KOTONOHA_BUILD_GAME\s+STREQUAL\s+game_id\s*\)",
            "canonical Android game matrix selection missing",
            "KOTONOHA_BUILD_GAME",
        )
        self.require(
            "CMakeLists.txt", r"include\s*\([^)]*KotonohaAndroid\.cmake",
            "engine root must configure Android before project()", "KotonohaAndroid",
        )
        self.require(
            "buildsystems/cmake/KotonohaAndroid.cmake",
            r"\bKOTONOHA_BUILD_GAME\b",
            "canonical Android toolchain selector missing",
            "KOTONOHA_BUILD_GAME",
        )
        types = braced(self.files[custom], r"\bbuildTypes\s*\{")
        if types is None:
            self.fail(custom, "missing School Days DEV/DIST buildTypes", "buildTypes")
        else:
            checks = {
                "dev": (
                    r"\binitWith\s+debug", r"applicationIdSuffix\s+'\.dev'",
                    r"buildConfigField\s+'boolean'\s*,\s*'DISTRIBUTION_BUILD'"
                    r"\s*,\s*'false'", r"KOTONOHA_DISTRIBUTION_BUILD=OFF",
                    r"KOTONOHA_DEV_CONTROL=OFF",
                ),
                "dist": (
                    r"\binitWith\s+release",
                    r"buildConfigField\s+'boolean'\s*,\s*'DISTRIBUTION_BUILD'"
                    r"\s*,\s*'true'", r"KOTONOHA_DISTRIBUTION_BUILD=ON",
                    r"KOTONOHA_DEV_CONTROL=OFF",
                ),
                "distTest": (
                    r"\binitWith\s+dist",
                    r"applicationIdSuffix\s+'\.test'",
                    r"\bsigningConfig\s+signingConfigs\.debug",
                    r"buildConfigField\s+'boolean'\s*,\s*'DISTRIBUTION_BUILD'"
                    r"\s*,\s*'true'",
                ),
            }
            for name, patterns in checks.items():
                section = braced(types, rf"\b{name}\s*\{{")
                if section is None or any(
                    not re.search(pattern, section, re.S) for pattern in patterns
                ):
                    self.fail(custom, f"schoolDays{name} build/signing "
                              "contract broken", name)
        filter_body = braced(self.files[custom], r"\bvariantFilter\s*\{")
        if not filter_body or not all(
            re.search(pattern, filter_body, re.S) for pattern in (
                r"schoolDaysVariant\s*&&\s*!\s*\(variant\.buildType\.name"
                r"\s+in\s+\['dist',\s*'distTest',\s*'dev'\]\)",
                r"!schoolDaysVariant\s*&&\s*\(variant\.buildType\.name"
                r"\s+in\s+\['dist',\s*'distTest',\s*'dev'\]\)",
                r"variant\.setIgnore\s*\(\s*true\s*\)",
            )
        ):
            self.fail(custom, "variantFilter must isolate School Days "
                      "Dev/Dist/DistTest", "variantFilter")
        self.require(
            f"{CUSTOM}/{DEV_FIXTURE}", r"\bBuildConfig\.DISTRIBUTION_BUILD\b",
            "DEV save fixture installer must reject distributions",
            "DISTRIBUTION_BUILD",
        )
        self.require(
            custom,
            r"buildConfigField\s+'boolean'\s*,\s*'DISTRIBUTION_BUILD'"
            r"\s*,\s*'false'",
            "base custom build must default to non-distribution mode",
            "DISTRIBUTION_BUILD",
        )
        source_sets = braced(self.files[custom], r"\bsourceSets\s*\{")
        main_sources = braced(source_sets or "", r"\bmain\s*\{")
        if main_sources is None or re.search(
            r"schoolDaysDev|SchoolDaysDevSaveFixtures", main_sources
        ):
            self.fail(
                custom, "DEV-only Java fixtures must not enter the main "
                "source set shared with DIST", "sourceSets",
            )

    def assets_and_workflow(self) -> None:
        copy = self.files[SHARED_COPY]
        function = re.search(
            r"\bfunction\s*\(\s*kotonoha_copy_assets\s+target\s*\)", copy
        )
        if not function or not re.match(
            r"\s*if\s*\(\s*ANDROID\s*\)\s*return\s*\(\s*\)\s*"
            r"endif\s*\(\s*\)", copy[function.end():], re.S,
        ):
            self.fail(
                SHARED_COPY, "kotonoha_copy_assets must return immediately "
                "on Android: only Gradle may stage APK assets",
                "function(kotonoha_copy_assets",
            )
        generic = f"{UPSTREAM}/app/build.gradle"
        self.require(
            generic,
            r"def\s+includeThisGame\s*=\s*"
            r"(?:variant\.flavorName\s*!=\s*'schoolDays'\s*&&\s*\(\s*)?"
            r"includeGameAssets\s*\|\|\s*"
            r"variant\.flavorName\s*==\s*'example'\s*\)?",
            "generic game assets must honor -PbundleGameAssets=false",
            "includeThisGame",
        )
        self.require(
            generic, r"if\s*\(\s*includeThisGame\s*\)\s*\{\s*"
            r"from\s*\(\s*gameAssets\s*\)",
            "generic game-assets copy must be opt-in", "includeThisGame",
        )
        custom = f"{CUSTOM}/app/build.gradle"
        stage = braced(
            self.files[custom],
            r"tasks\.register\(['\"]stage\${taskSuffix}Assets['\"],\s*Sync\)\s*\{",
        )
        school = braced(
            stage or "", r"if\s*\(\s*variant\.flavorName\s*==\s*"
            r"'schoolDays'\s*\)\s*\{",
        )
        if not school:
            self.fail(custom, "missing isolated School Days asset staging",
                      "stage")
            return
        route = braced(school, r"from\s*\(\s*gameAssets\s*\)\s*\{")
        if route is None or set(included_patterns(route)) != {
            "school-days-hq.ktnroute", "styles.ass"
        } or len(included_patterns(route)) != 2:
            self.fail(custom, "only route/styles may come from School Days "
                      "game-assets; no retail GPK", "from(gameAssets)")
        art = braced(
            school, r"from\s*\(\s*new\s+File\s*\(\s*engineRoot\s*,\s*"
            r"'games/school-days/game-assets/launcher-art'\s*\)\s*\)\s*\{",
        )
        if art is None or included_patterns(art) != ["SchoolDaysHQ-Launcher.png"]:
            self.fail(custom, "launcher art must be an explicit PNG allowlist",
                      "launcher-art")
        translations = braced(
            school, r"from\s*\(\s*new\s+File\s*\(\s*"
            r"schoolDaysTranslationsDir\s*\)\s*\)\s*\{",
        )
        if translations is None or (
            set(included_patterns(translations)) != TRANSLATION_INCLUDES or
            len(included_patterns(translations)) != len(TRANSLATION_INCLUDES)
        ):
            self.fail(custom, "external translation allowlist must exclude "
                      "original HQ Packs", "schoolDaysTranslationsDir")
        dev = braced(
            school, r"if\s*\(\s*variant\.name\s*==\s*'schoolDaysDev'\s*"
            r"&&\s*schoolDaysDevSavesDir\s*\)\s*\{",
        )
        saves = braced(
            dev or "", r"from\s*\(\s*new\s+File\s*\(\s*"
            r"schoolDaysDevSavesDir\s*\)\s*\)\s*\{",
        )
        if saves is None or set(included_patterns(saves)) != DEV_SAVES or (
            len(included_patterns(saves)) != len(DEV_SAVES)
        ):
            self.fail(custom, "only schoolDaysDev may stage eight external "
                      "test saves", "schoolDaysDevSavesDir")
        if len(re.findall(r"\bfrom\s*\(", school)) != 4:
            self.fail(custom, "unapproved additional School Days asset "
                      "source: possible GPK/save leakage", "from(gameAssets)")
        # Four School Days sources plus shared assets and the other-game source
        # use from(...). DDLC's standard library uses bare "from libraryRoot".
        # Any additional from(...) could bypass the commercial-data allowlist.
        if len(re.findall(r"\bfrom\s*\(", stage or "")) != 6:
            self.fail(custom, "unexpected extra Android staging source: "
                      "inspect for unapproved GPK/save copy", "stage")
        if not re.search(
            r"\}\s*else\s+if\s*\(\s*includeThisGame\s*\)\s*"
            r"\{\s*from\s*\(\s*gameAssets\s*\)", stage or "", re.S,
        ):
            self.fail(custom, "other custom flavors must obey "
                      "-PbundleGameAssets", "includeThisGame")
        for pattern, reason in (
            (r"if\s*\(\s*!schoolDaysTranslationsDir\s*\)",
             "School Days builds must require external translations"),
            (r"if\s*\(\s*!schoolDaysDevSavesDir\s*\)",
             "DEV builds must require external save fixtures"),
            (r"schoolDaysDevExpectedSaveHashes\[name\]",
             "DEV saves must be SHA-256 pinned"),
        ):
            self.require(custom, pattern, reason, "doFirst")
        keys_match = re.search(
            r"def\s+schoolDaysDevExpectedSaveHashes\s*=\s*\[([^\]]+)\]",
            self.files[custom], re.S,
        )
        names = (
            re.findall(
                r"'(SaveFile[0-9]{3}\.DAT|GlobalFlag\.DAT)'\s*:\s*"
                r"'[a-f0-9]{64}'", keys_match.group(1),
            ) if keys_match else []
        )
        if len(names) != len(DEV_SAVES) or set(names) != DEV_SAVES:
            self.fail(custom, "DEV save fixtures need eight distinct pinned "
                      "SHA-256 identities", "schoolDaysDevExpectedSaveHashes")
        self.require(
            "lib/CMakeLists.txt", r"add_library\s*\(\s*imgui\s+",
            "native imgui target name must match getLibraries()",
            "add_library(imgui",
        )
        self.require(
            WORKFLOW, r"python3\s+tools/check_hybrid_android_contracts\.py",
            "Android CI must invoke this optional static contract", "Validate",
        )
        self.require(
            WORKFLOW, r"--overlay-ports=\$GITHUB_WORKSPACE/vcpkg-overlays/ports",
            "CI vcpkg overlay must match both Gradle projects", "--overlay-ports",
        )
        self.require(
            WORKFLOW, r"gradle\s+:app:assembleDebug\s+\\\s*"
            r"-Pabis=arm64-v8a\s+-PbundleGameAssets=false",
            "CI must keep commercial assets out of generic APK builds",
            "assembleDebug",
        )

    def run(self) -> Tuple[List[str], bool]:
        custom = (
            (self.root / CUSTOM).is_dir()
            if self.custom_exists is None else self.custom_exists
        )
        if not custom:
            return [], False
        for path in REQUIRED:
            try:
                data = (
                    self.overrides[path] if path in self.overrides
                    else (self.root / path).read_text(encoding="utf-8")
                )
                self.files[path] = strip_comments(data)
            except (OSError, UnicodeError) as error:
                self.fail(path, f"required source missing or unreadable: {error}")
        if self.problems:
            return self.problems, True
        self.project(UPSTREAM, True)
        self.project(CUSTOM, False)
        self.selectors_and_variants()
        self.assets_and_workflow()
        return self.problems, True


def self_test(root: Path, baseline: Checker) -> None:
    """Mutation tests operate only on in-memory source strings."""
    cases = (
        (f"{CUSTOM}/{ACTIVITY}", '"imgui"', '"kotonoha_imgui"',
         "getLibraries"),
        (f"{UPSTREAM}/{ACTIVITY}", '"imgui"', '"kotonoha_imgui"',
         "getLibraries"),
        (f"{CUSTOM}/{PROGUARD}",
         "-keep class org.kotonoha.android.GameActivity",
         "-dontwarn class org.kotonoha.android.GameActivity", "R8"),
        (f"{UPSTREAM}/{PROGUARD}",
         "-keep class org.kotonoha.android.GameActivity",
         "-dontwarn class org.kotonoha.android.GameActivity", "R8"),
        (f"{CUSTOM}/app/build.gradle", "vcpkg-overlays/ports",
         "vcpkg-overlays", "vcpkg overlay"),
        (f"{UPSTREAM}/app/build.gradle", "vcpkg-overlays/ports",
         "vcpkg-overlays", "vcpkg overlay"),
        (f"{UPSTREAM}/app/build.gradle", "applicationIdSuffix '.upstream'",
         "applicationIdSuffix '.collision'", ".upstream"),
        (f"{CUSTOM}/CMakeLists.txt",
         'set(KOTONOHA_BUILD_GAME "${KOTONOHA_ANDROID_GAME}"',
         'set(KOTONOHA_BUILD_GAME "${BROKEN_GAME}"', "bridge"),
        (f"{CUSTOM}/app/build.gradle", "applicationIdSuffix '.dev'",
         "applicationIdSuffix '.collision'", "schoolDaysdev"),
        (f"{CUSTOM}/app/build.gradle", "KOTONOHA_DISTRIBUTION_BUILD=ON",
         "KOTONOHA_DISTRIBUTION_BUILD=OFF", "schoolDaysdist"),
        (f"{CUSTOM}/app/build.gradle",
         "if (variant.name == 'schoolDaysDev' && schoolDaysDevSavesDir)",
         "if (variant.name == 'schoolDaysDist' && schoolDaysDevSavesDir)",
         "schoolDaysDev"),
        (f"{CUSTOM}/app/build.gradle",
         "include 'school-days-hq.ktnroute', 'styles.ass'",
         "include '**/*.GPK', 'styles.ass'", "GPK"),
        (SHARED_COPY, "if(ANDROID)\n        return()",
         "if(NOT ANDROID)\n        return()", "Gradle"),
        (WORKFLOW, "-PbundleGameAssets=false",
         "-PbundleGameAssets=true", "commercial assets"),
    )
    for path, original, changed, why in cases:
        source = baseline.files[path]
        if source.count(original) != 1:
            raise AssertionError(
                f"mutation fixture is missing or ambiguous: {path}: {original}"
            )
        mutated = source.replace(original, changed, 1)
        failures, present = Checker(
            root, overrides={path: mutated}, custom_exists=True
        ).run()
        if not present or not failures:
            raise AssertionError(f"mutation escaped validation: {why} in {path}")
    failures, present = Checker(root, custom_exists=False).run()
    if present or failures:
        raise AssertionError("upstream-only checkout must skip hybrid checks")
    print(f"[Hybrid Android] self-test PASS: {len(cases)} in-memory "
          "regressions detected; upstream-only checkout accepted")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true",
                        help="exercise deliberate in-memory contract regressions")
    args = parser.parse_args()
    checker = Checker(args.root.resolve())
    problems, hybrid = checker.run()
    if problems:
        for issue in problems:
            print("[Hybrid Android] FAIL " + issue)
        return 1
    if hybrid:
        print("[Hybrid Android] PASS: independent Gradle projects, vcpkg, "
              "JNI/R8, CMake bridge, DEV/DIST, and safe asset ownership")
        if args.self_test:
            self_test(args.root.resolve(), checker)
    else:
        print("[Hybrid Android] SKIP: custom Android/ absent (upstream-only)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
