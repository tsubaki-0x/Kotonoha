# Kotonoha Android

**English (US)** | [Português (Brasil)](README.pt-BR.md)

Open this folder in Android Studio. Required: **JDK 17**, Android **SDK 36**, **NDK 28.2.13676358**, **CMake 3.22.1**, Git, and vcpkg. This is the generic upstream CI project; the hybrid's School Days DEV/DIST variants live in `../../Android/`. The wrapper scripts do not include `gradle-wrapper.jar`, so use installed Gradle or Android Studio unless a trusted wrapper JAR is supplied.

```sh
export VCPKG_ROOT=/path/to/vcpkg
gradle :app:assembleExampleDebug
gradle :app:assembleDokiDokiDebug -Pabis=arm64-v8a
gradle :app:assembleSchoolDaysDebug -Pabis=armeabi-v7a -PbundleGameAssets=false
```

Run commands from `buildsystems/android/` (PowerShell: set `$env:VCPKG_ROOT` and use installed `gradle`). The generic School Days APK uses an `.upstream` application-ID suffix, avoiding collisions with hybrid retail installs. Other flavors include `Amagami`, `ZeroNoTsukaima`, `SnowRadish`, `SummerRadish`, `RealLive`, `Kirikiri`, `LucaSystem`, and `Pmail`.

- APKs are written under `app/build/outputs/apk/`. Default ABIs: `arm64-v8a,armeabi-v7a,x86_64`; use `-Pabis=armeabi-v7a` for ARMv7-only or `-Pabis=arm64-v8a` for ARM64-only APKs. ARMv7 requires NEON (`ANDROID_ARM_NEON=ON`, vcpkg `arm-neon-android`), so the device must support NEON.
- Each flavor has its own application ID; the root CMake selects only that game's native target.
- Gradle uses the SDL submodule and vcpkg for native dependencies. DDLC's CPython dependencies have an ABI-specific installation directory (`armeabi-v7a` uses `arm-neon-android`); prefer a single ABI for DDLC builds.
- Gradle bundles engine `assets/` and the selected game's `game-assets/`; `-PbundleGameAssets=false` excludes commercial game data. Packaged resources are extracted on startup and saves stay separate.
- Do not launch concurrent Gradle invocations that compete for the same `.cxx` directory.

For external assets, overlays, OBB, and saved files, see the [directory policy](../../docs/DIRECTORY_POLICY.md). See also [Android build guide](../../docs/BUILD_ANDROID.md).
