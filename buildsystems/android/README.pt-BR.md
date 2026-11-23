# Kotonoha Android

[English (US)](README.md) | **Português (Brasil)**

Abra a pasta no Android Studio. Requisitos: **JDK 17**, Android **SDK 36**, **NDK 28.2.13676358**, **CMake 3.22.1**, Git e vcpkg. Este e o projeto generico de CI upstream; as edicoes DEV/DIST do School Days da hibrida ficam em `../../Android/`. Os scripts de wrapper nao incluem `gradle-wrapper.jar`; utilize Gradle instalado ou Android Studio ate fornecer um JAR confiavel.

```sh
export VCPKG_ROOT=/caminho/para/vcpkg
gradle :app:assembleExampleDebug
gradle :app:assembleDokiDokiDebug -Pabis=arm64-v8a
gradle :app:assembleSchoolDaysDebug -Pabis=armeabi-v7a -PbundleGameAssets=false
```

Execute dentro de `buildsystems/android/` (PowerShell: defina `$env:VCPKG_ROOT` e use `gradle` instalado). O APK generico School Days usa sufixo de aplicativo `.upstream` para nao colidir com instalacoes retail da hibrida. Outros flavors: `Amagami`, `ZeroNoTsukaima`, `SnowRadish`, `SummerRadish`, `RealLive`, `Kirikiri`, `LucaSystem` e `Pmail`.

- APKs ficam em `app/build/outputs/apk/`. ABIs padrão: `arm64-v8a,armeabi-v7a,x86_64`; use `-Pabis=armeabi-v7a` para APK só ARMv7 ou `-Pabis=arm64-v8a` para ARM64. O ARMv7 exige NEON (`ANDROID_ARM_NEON=ON`, triplet vcpkg `arm-neon-android`); o aparelho precisa ter NEON.
- Cada flavor tem seu ID próprio e o CMake compila apenas o alvo nativo correspondente.
- O Gradle usa o submódulo SDL e dependências do vcpkg. DDLC usa CPython por ABI (`armeabi-v7a` usa `arm-neon-android`); prefira uma ABI por build.
- Gradle inclui `assets/` do motor e `game-assets/` do jogo; `-PbundleGameAssets=false` exclui os dados comerciais. Resources empacotados são extraídos no início; saves ficam separados.
- Não execute dois Gradles simultaneamente no mesmo diretório nativo `.cxx`.

Para dados externos, overlays, OBB e saves, consulte a [política de diretórios](../../docs/DIRECTORY_POLICY.pt-BR.md) e o [guia Android](../../docs/BUILD_ANDROID.pt-BR.md).
