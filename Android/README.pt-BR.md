# Kotonoha Android

[English](README.md) | Portugues (Brasil)

Este projeto Android personalizado da hibrida conserva as edicoes School Days
DEV, DIST e DIST-TEST. O projeto generico upstream de CI em
`../buildsystems/android/` tambem e preservado.
Abra esta pasta no Android Studio. Requisitos: JDK 17 ou mais recente,
Gradle 8.14.6, SDK 37 (minor 0), CMake 3.22.1, NDK 27.0.12077973,
Git e um checkout vcpkg.

```sh
export VCPKG_ROOT=/caminho/para/vcpkg
export ANDROID_HOME=/caminho/para/Android/sdk
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.0.12077973"
export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
gradle :app:assembleExampleDebug -Pabis=arm64-v8a
gradle :app:assembleZeroNoTsukaimaDebug -Pabis=arm64-v8a
gradle :app:assembleSnowRadishDebug -Pabis=arm64-v8a
gradle :app:assembleDokiDokiDebug -Pabis=arm64-v8a
gradle :app:assembleSchoolDaysDistTest -Pabis=arm64-v8a \
  -PschoolDaysTranslationsDir=/traducoes/externas/autorizadas
gradle :app:assembleSchoolDaysDistTest -Pabis=armeabi-v7a \
  -PschoolDaysTranslationsDir=/traducoes/externas/autorizadas \
  -PbundleGameAssets=false
```

No Windows, use PowerShell na pasta `Android`:

```powershell
$env:VCPKG_ROOT = 'C:\caminho\para\vcpkg'
$env:JAVA_HOME = 'C:\Program Files\Android\Android Studio\jbr'
gradle :app:assembleExampleDebug -Pabis=arm64-v8a
```

O vcpkg instalado com o Visual Studio tambem pode ser usado. Ao abrir o
Android Studio pelo menu Iniciar, configure `VCPKG_ROOT` nas variaveis de
ambiente do usuario e reinicie o Studio. O SDK vem de `local.properties`
(gerado pelo Studio); o NDK selecionado pelo Gradle e repassado ao vcpkg.
Nao e necessario definir `ANDROID_NDK_HOME` separadamente.

Cada flavor tem applicationId e APK proprios em `app/build/outputs/apk/`.
Selecione **uma unica ABI** por compilacao: `arm64-v8a` (padrao),
`armeabi-v7a` (ARM 32 bits com NEON) ou `x86_64` (emulador).
Listas com virgulas sao rejeitadas; os triplets sao `arm64-android`,
`arm-neon-android` e `x64-android`. O ARMv7 exige NEON no dispositivo,
e `ANDROID_ARM_NEON=ON` e aplicado antes de configurar as dependencias.
School Days exige `-PschoolDaysTranslationsDir=...` com traducoes fora do
repositorio; DEV exige ainda `-PschoolDaysDevSavesDir=...` com os oito hashes
autorizados, nunca uma pasta de saves pessoais. DIST-TEST possui assinatura de
teste; DIST de producao exige assinatura release. Os scripts de wrapper nao
incluem o JAR, portanto use Gradle instalado ou Android Studio ate fornecer um
wrapper JAR de origem confiavel.
Execute as invocacoes Gradle sequencialmente: builds simultaneos compartilham
os diretorios nativos `.cxx`. Uma invocacao pode receber varios targets assemble.
Compile DDLC com uma ABI por APK. As dependencias ficam em `build/vcpkg/<ABI>`; DDLC usa CPython fornecido pelo vcpkg. Nenhuma biblioteca Python macOS/iOS e usada no Android.

Os fontes Java SDLActivity e a biblioteca nativa SDL usam o mesmo checkout
em `lib/submodules/SDL`.
A engine e empacotada como `libkotonoha_engine.so`, com nome distinto de
`libKotonoha.so` do exemplo mesmo em sistemas de arquivos sem distincao de caixa.
SDL_ttf e ImGui continuam nos submodulos; FFmpeg, json-c, libass, iconv, PlutoSVG e
CPython opcional sao fornecidos pelo vcpkg.
DDLC usa uma instalacao separada em `build/vcpkg-python/<ABI>` para que configurar
outros jogos nao remova sua dependencia opcional de CPython.
Cada ABI possui sua propria instalacao para que o vcpkg nao remova as
bibliotecas da outra arquitetura ao configurar um APK com duas ABIs.
O manifesto fixa `vcpkg-tool-meson` em 1.9.0#10: o Meson 1.12.1 rejeita
prefixos de instalacao Windows (`C:/...`) ao compilar para Android.

Cada APK inclui `assets/` do motor e `game-assets/` do jogo escolhido.
Gradle prepara os recursos e calcula sua identidade pelo conteudo.
BootstrapActivity extrai o APK em uma thread de trabalho antes de iniciar
SDL e mostra erros de preparo. `-PbundleGameAssets=false` exclui dados
comerciais locais. Forneca dados obtidos legitimamente em
`../games/<jogo>/game-assets/`; nao os coloque no Git.

Os assets da engine sao extraidos em `<dataDir>/files/assets`; os do jogo em
`<dataDir>/game_assets`. Dados externos podem ficar em
`Android/obb/<applicationId>/game_assets`, `game-assets` ou diretamente no OBB,
com prioridade sobre os dados empacotados. Saves e configuracoes usam
`Android/data/<applicationId>/files/Kotonoha/<applicationId>`.
O launcher salva nomes de recursos e resolve seus caminhos em cada execucao.
`overlay` e descoberto automaticamente; `overlay2` selecionado no launcher
pode estar em OBB, dados externos ou dados privados da aplicacao.
Veja a [politica completa de diretorios](../docs/Directories.pt-BR.md).
O armazenamento com escopo pode limitar acesso de gerenciadores de arquivos;
nao solicitamos acesso amplo nem criamos uma pasta artificial `Android/obb-data`.

Os recursos extraidos sao privados, em caminhos estaveis; seu fingerprint
determina quando atualizar os arquivos a partir do APK.
Dialogos nativos podem estar indisponiveis no Android; o inspetor MPV tambem
oferece selecao de shaders incluidos no pacote.
