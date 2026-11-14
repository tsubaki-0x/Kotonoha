# Relatorio de integracao upstream e distribuicao Windows L4

Data: 2026-11-13. Escopo executado: School Days HQ v1.02 no Windows L4. Nenhum push, Release remota ou PR foi criado.

## Repositorios, comparacao e backup

| Item | Resultado |
| --- | --- |
| Clone | `D:\Dev\Kotonoha-v3-fonte` |
| Origin | `https://github.com/tsubaki-0x/Kotonoha.git` |
| Upstream | `https://github.com/Ruaneri-Portela/Kotonoha.git` |
| Branch inicial | `kotonoha-v3-ruaneri-sync-transitions` |
| HEAD inicial e backup imutado `backup/pre-sync-2026-11-13` | `9e0e080063286220b0acf4e165dc259d54e41b20` |
| Merge-base | `9e751c8bef391179f549b0082ae8674275252b6f` |
| Upstream incorporado | `451b070c5b5265a4e8aa9c0ede35a12f9f78142e` |
| Branch de trabalho | `sync-upstream-2026-11-13` |
| Merge local | `bc6a3480bf99147f5ef8efbcca53b57442f847ba` (pais: HEAD inicial e upstream) |

Antes do merge, a comparacao correta apos aprofundar o historico raso mostrou **184 commits exclusivos do fork**, **54 exclusivos do upstream** e **814 caminhos divergentes**. Os submodulos SDL, SDL_ttf e ImGui estavam inteiros e permaneceram nos commits do fork. `git fetch --all` foi tentado; a primeira transferencia nao filtrada se mostrou excessiva e foi interrompida. A variante com `--filter` nao era aceita pelo remote sem `extensions.partialclone`; por isso cada remote foi buscado separadamente, com filtro e aprofundamento de 50 commits apenas ate localizar o merge-base.

RTK foi usado para resumir `git status`, `git log`, `git diff` e outras saidas compativeis; comandos Git nativos ficaram para operacoes e verificacoes que exigiam a saida completa. `rtk gain` registrou economia global de 19,2% de tokens nesta sessao.

Os commits upstream de dublagem identificados foram `a10a0f6` (`Dub system and translation fixes`) e `c5f933d` (`Dub tools`). Eles introduzem `VoiceDubbingApplet`: descoberta de falas, comparacao com audio original, importacao e gravacao por microfone, preview e edicao de waveform, gravacao WAV em overlay e descarte/restauracao. Usa SDL3, ImGui, decodificacao de audio e o provedor generico de assets; nao requer editor externo para funcionar. No desenvolvimento a entrada fica no inspetor F10; na distribuicao Windows L4, F10 abre diretamente a janela Dubbing.

## Merge e decisoes de conflito

O merge `--no-ff --no-commit` apresentou **150 conflitos**: 92 em `games/school-days/`, 40 em `lib/` e 18 em raiz, Android, CMake, assets e dependencias. A lista completa de caminhos esta no apendice. A resolucao foi por dominio, examinando as implementacoes relevantes, e nao por escolha global de um dos lados:

| Grupo | Decisao e razao |
| --- | --- |
| School Days, GPK, JSON, launcher, saves, Config.DAT, LxH, display, audio e video | Preservada a implementacao Windows L4 do fork. O upstream reorganiza esses arquivos e nao cobre as customizacoes retail/L4 do fork. Foram portadas apenas as interfaces necessarias para dublagem, inclusive leitura de voz original sem overlay. |
| `lib/` core, componentes, debug e testes | Preservado o motor do fork; incorporadas as APIs genericas de audio original/efetivo, overlay, lista de audio recente, applet e tema ImGui. O applet foi ligado ao inspetor existente sem substituir a interface do fork. |
| CMake, `cmake/`, `Android/`, assets e vcpkg | Preservados os caminhos e configuracao de build do fork para nao romper o Windows L4. Adicionada a opcao explicita `KOTONOHA_ENABLE_PLAYER_DUBBING` para a distribuicao. Android nao foi compilado nem iniciado. |
| Reorganizacao upstream em `buildsystems/`, `modules/`, outros jogos e Xcode | Arquivos novos upstream foram incorporados como fonte; 96 caminhos antigos apagados/renomeados pelo merge foram restaurados do backup para manter as referencias do CMake do fork. Essa arquitetura adicional nao foi ativada nem validada fora do alvo Windows L4. |

O diff final do merge contra o backup tem **427 caminhos: 308 adicionados e 119 modificados**, aproximadamente **56.607 linhas adicionadas e 1.174 removidas**. Nao houve caminho antigo removido. A arvore ficou sem conflitos pendentes. Os arquivos locais da tarefa e do iniciador ficaram fora do commit.

## Ajustes feitos

- O provedor generico distingue asset original e asset com overlay; a dublagem pode comparar e reproduzir ambos. A consulta de voz original do School Days ignora tanto overlay de arquivos quanto resolucao global do GPK.
- A lista de falas da cena usa eventos `PLAY_VOICE` e mostra personagem/texto disponivel. A troca ou descarte de dublagem invalida midia preparada para que a reproducao seguinte use a escolha atual.
- F10 abre a janela Dubbing na build de distribuicao; Esc fecha. O inspetor completo e MCP continuam apenas na build de desenvolvimento. O motor pausa a historia enquanto a janela esta aberta.
- O WAV salvo fica no overlay local do jogo, nunca em `Packs`. IDs de saida e caminho canonico sao validados; um WAV anterior e renomeado para `.bak-N` antes da substituicao. `--documents` redireciona tambem o overlay de dublagem para testes isolados.
- O perfil de saves generico do fork foi preservado. `settings.json` guarda ajustes do launcher/motor; `Config.DAT` e preferencias Display/General continuam no perfil do School Days. A distribuicao aceita `--config` e `--documents` para QA isolado.
- Logs de informacao/debug do motor e do parser ASS foram desativados na distribuicao, preservando erros. O arquivo `LEIA-ME-DUBLAGEM.txt` na nova distribuicao explica os controles.
- Um teste de listagem passou a contar o novo arquivo de voz da fixture. O teste do splash foi alinhado ao comportamento anterior do fork: o logo usa amostragem de UI e nao o upscaler de gameplay.

## Build e verificacoes

| Tipo | Evidencia e resultado |
| --- | --- |
| Dependencias | vcpkg real em `D:\vcpkg`, triplet `x64-windows-static-md`, 22 dependencias instaladas a partir de cache local apenas nos builds. O `VCPKG_ROOT` antigo apontava para caminho inexistente e foi corrigido apenas para os comandos da sessao. |
| Build dev | `cmake --build build/school-days-l4-two-dll-staticmd --config Release --parallel 1` — passou apos as correcoes. |
| Build distribuicao | `build/school-days-l4-dubbing-dist-staticmd`, Release, `KOTONOHA_DISTRIBUTION_BUILD=ON`, `KOTONOHA_ENABLE_PLAYER_DUBBING=ON`, School Days ON, demais jogos OFF — passou. |
| CTest / unitarios | `games/school-days/tools/stress_l4.py --build-dir build/school-days-l4-two-dll-staticmd --cycles 1 --smokes 4 --timeout 25` — **74/74 testes**, **4/4 smokes sinteticos** de software, `GPK_UNCHANGED=True`. Relatorio detalhado em `build/school-days-l4-two-dll-staticmd/qa-stress/stress-20261113-211427/stress-report.json`. |
| Regressao investigada | CTest sem as DLLs no `PATH` parou em executaveis sem dependencias carregaveis. Com as DLLs no `PATH`, inicialmente 71/74 passaram; os tres testes afetados foram corrigidos e passaram individualmente, depois a suite oficial passou 74/74. |
| Dependencias Windows | `cmake/PackageWindowsRuntime.cmake` executou `file(GET_RUNTIME_DEPENDENCIES)` sobre o EXE na nova pasta e nao encontrou DLL externa nao resolvida. |
| GPU real / inicializacao | O EXE da pasta nova, com `--school-days`, `--documents` e `--config` isolados em `build/`, manteve o jogo ativo por 8 s com renderer OpenGL real; o launcher iniciou com titulo `School Days HQ - Launcher`. Stderr final vazio. Cada processo de teste foi encerrado antes de iniciar outro. |
| QA visual/manual e audio | Nao foi feita inspeccao visual da janela Dubbing, teste de microfone, preview ouvido nem percurso completo do jogo. Integracao da UI foi confirmada por codigo/build; sua operacao interativa exige QA manual. |

O teste de jogo com `SDL_VIDEODRIVER=dummy` falhou na criacao do OpenGL, como o diagnostico informou; por isso foi repetido com GPU Windows real e ambiente de dados isolado. Permanecer ativo demonstra inicializacao, nao valida todas as cenas ou portabilidade em outro computador.

## Nova distribuicao e Packs

Destino unico criado: `D:\Dev\Kotonoha-v3-fonte\dist\Kotonoha-2026-11-13`. Contem `SchoolDays.exe` Release, `SDL3.dll`, `SDL3_ttf.dll`, assets, display-options, game-assets, traducoes e `Packs/`. Sao **119 arquivos**, **12.260.076.458 bytes**. SHA-256 do EXE final: `21E3993556ACCCD3EC8508748978BE78E7D201E28B51A9651A942929E59317F4`.

Os **29 GPKs retail** somavam **11.958.185.811 bytes**. Todos os nomes, tamanhos e SHA-256 das 29 copias coincidem com a origem; a nova pasta `Packs/` tem 30 GPKs incluindo `LxH.GPK`. O `LxH.GPK` da fonte de pre-requisitos e da distribuicao tem SHA-256 `772B501FC70AE89E52861378A99FF1B27D3C47EB20B2473836591069143EDB64`. Os manifestos locais estao em `build/source-packs-sha256.csv` e `build/dist-packs-sha256.csv`. Os GPKs originais foram apenas lidos; nenhum foi movido ou sobrescrito. A distribuicao antiga em `D:\Dev\Kotonoha-v3\dist\SchoolDaysHQ-Windows-Clean-Distribution-v3` permaneceu intocada.

Treze recursos ausentes no clone vieram dos pre-requisitos locais; 54 arquivos existentes foram preservados sem sobrescrita. Traducoes privadas e assets comerciais da distribuicao estao em `dist/` ignorado pelo Git; nenhum GPK, `Config.DAT`, save real ou traducao privada entrou no commit. O build usa runtime MSVC dinamico (`static-md`); a presenca do redistribuivel Visual C++ em outro computador e a portabilidade fora desta maquina ainda precisam ser verificadas.

## Pendencias e riscos conhecidos

1. Fazer QA visual/manual da janela F10, importacao/gravacao de microfone, waveform, salvar, descartar e reproducao da voz no jogo. Nenhuma afirmacao de audio ouvido foi feita.
2. Validar o pacote em outra instalacao Windows L4 com o redistribuivel Visual C++ apropriado e GPU/driver disponiveis.
3. Compilar e testar separadamente outros jogos/plataformas se a reorganizacao upstream for ativada. O trabalho presente concluiu somente o alvo School Days Windows L4.
4. `git diff --check` ainda aponta espacos em branco preexistentes em arquivos novos do upstream, sobretudo patches FFmpeg e Pmail. Os arquivos de integracao editados nesta tarefa passaram pela verificacao de whitespace.

## Apendice A: caminhos que apresentaram conflito

```text
.gitattributes
.gitignore
Android/app/build.gradle
Android/app/src/main/java/org/kotonoha/android/GameActivity.java
CMakeLists.txt
assets/SPLASH.PNG
buildsystems/android/app/proguard-rules.pro
buildsystems/android/app/src/main/java/org/kotonoha/android/AndroidImmersion.java
buildsystems/android/app/src/main/java/org/kotonoha/android/SchoolDaysEmbeddedTranslations.java
buildsystems/android/app/src/main/java/org/kotonoha/android/SchoolDaysMobileDisplayOptions.java
buildsystems/android/app/src/main/java/org/kotonoha/android/SchoolDaysMobileProfile.java
buildsystems/android/app/src/schoolDaysDev/java/org/kotonoha/android/SchoolDaysDevSaveFixtures.java
buildsystems/android/gradle.properties
buildsystems/cmake/PackageWindowsRuntime.cmake
cmake/KotonohaApp.cmake
games/school-days/CMakeLists.txt
games/school-days/docs/RELEASE_READINESS_AUDIT.md
games/school-days/include/Kotonoha/SchoolDaysAudioEventState.hpp
games/school-days/include/Kotonoha/SchoolDaysMoveSomState.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysAbcAnimation.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysBackLog.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysBackLogPresenter.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysChoiceTextPlan.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysConfigMenuModel.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysConfigMenuRenderPlan.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysConfigStorage.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysDesktopStartupProfile.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysDisplayPolicy.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysEpisodeOverlay.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysFrontendTransition.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysInputPolicy.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysLoadTransition.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysMenuBarPresenter.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysOrsFadeBoundary.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysOrsFadeFrame.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysPersistence.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysPlayDataSession.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysPlaybackPolicy.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysPromptBehavior.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysReplayModel.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysReplayPlaybackSession.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysReplayScriptOracle.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysRouteMapRenderPlan.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysSaveLoadModel.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysSaveStorage.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysSubtitleTypography.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysSystemPanelTransition.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysSystemUi.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysTitleMovieClock.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysTopMeterModel.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysTopMeterRenderPlan.hpp
games/school-days/include/Kotonoha/routing/SchoolDaysTransportFade.hpp
games/school-days/src/SchoolDaysAssets.cpp
games/school-days/src/SchoolDaysAssets.hpp
games/school-days/src/SchoolDaysChoiceTextPlan.cpp
games/school-days/src/SchoolDaysConfigMenuRenderPlan.cpp
games/school-days/src/SchoolDaysConfigStorage.cpp
games/school-days/src/SchoolDaysDisplayPolicy.cpp
games/school-days/src/SchoolDaysMenuBarLayout.cpp
games/school-days/src/SchoolDaysMenuBarPresenter.cpp
games/school-days/src/SchoolDaysPlayDataSession.cpp
games/school-days/src/SchoolDaysReplayPlaybackSession.cpp
games/school-days/src/SchoolDaysReplayScriptOracle.cpp
games/school-days/src/SchoolDaysRouteMapRenderPlan.cpp
games/school-days/src/SchoolDaysRuntime.cpp
games/school-days/src/SchoolDaysRuntime.hpp
games/school-days/src/SchoolDaysSaveLoadModel.cpp
games/school-days/src/SchoolDaysSaveLoadRenderPlan.cpp
games/school-days/src/SchoolDaysScenes.cpp
games/school-days/src/SchoolDaysScenes.hpp
games/school-days/src/SchoolDaysSystemUi.cpp
games/school-days/src/SchoolDaysTopMeterModel.cpp
games/school-days/src/SchoolDaysTopMeterRenderPlan.cpp
games/school-days/src/main.cpp
games/school-days/tests/CMakeLists.txt
games/school-days/tests/SchoolDaysAssetsListTest.cpp
games/school-days/tests/SchoolDaysAudioEventStateTest.cpp
games/school-days/tests/SchoolDaysBackRuntimeTest.cpp
games/school-days/tests/SchoolDaysChoiceTextPlanTest.cpp
games/school-days/tests/SchoolDaysConfigMenuDisplayTest.cpp
games/school-days/tests/SchoolDaysConfigStorageTest.cpp
games/school-days/tests/SchoolDaysDisplayPolicyTest.cpp
games/school-days/tests/SchoolDaysEpisodeOverlayTest.cpp
games/school-days/tests/SchoolDaysExitRuntimeTest.cpp
games/school-days/tests/SchoolDaysFrontendTransitionTest.cpp
games/school-days/tests/SchoolDaysGpkStreamingTest.cpp
games/school-days/tests/SchoolDaysHandoffRuntimeTest.cpp
games/school-days/tests/SchoolDaysInputPolicyTest.cpp
games/school-days/tests/SchoolDaysLauncherTest.cpp
games/school-days/tests/SchoolDaysMediaTest.cpp
games/school-days/tests/SchoolDaysMenuBarLayoutTest.cpp
games/school-days/tests/SchoolDaysMenuBarRuntimeTest.cpp
games/school-days/tests/SchoolDaysNextChoiceRuntimeTest.cpp
games/school-days/tests/SchoolDaysPlayDataSessionTest.cpp
games/school-days/tests/SchoolDaysPortTest.cpp
games/school-days/tests/SchoolDaysPromptPolicyTest.cpp
games/school-days/tests/SchoolDaysReplayPlaybackSessionTest.cpp
games/school-days/tests/SchoolDaysReplayRuntimeIsolationTest.cpp
games/school-days/tests/SchoolDaysReplayRuntimeUiTest.cpp
games/school-days/tests/SchoolDaysReplayScriptOracleTest.cpp
games/school-days/tests/SchoolDaysRetailTest.cpp
games/school-days/tests/SchoolDaysRuntimeConfigStorageTest.cpp
games/school-days/tests/SchoolDaysSaveTest.cpp
games/school-days/tests/SchoolDaysSceneTimelineTest.cpp
games/school-days/tests/SchoolDaysSystemUiCommentEditorTest.cpp
games/school-days/tests/SchoolDaysTitleMovieClockTest.cpp
games/school-days/tests/SchoolDaysTopMeterModelTest.cpp
lib/CMakeLists.txt
lib/include/Kotonoha/components/DisplayModeCatalog.hpp
lib/include/Kotonoha/components/SelectBox.hpp
lib/include/Kotonoha/core/AssetProvider.hpp
lib/include/Kotonoha/core/Engine.hpp
lib/include/Kotonoha/core/Launcher.hpp
lib/include/Kotonoha/core/VisualTransition.hpp
lib/include/Kotonoha/mcp/MCPServer.hpp
lib/include/Kotonoha/media/MediaDecoder.hpp
lib/include/Kotonoha/media/MediaPlayer.hpp
lib/include/Kotonoha/media/TextureAlpha.hpp
lib/include/Kotonoha/mobile/AndroidDisplay.hpp
lib/src/componentes/SubtitleScene.cpp
lib/src/components/DisplayModeCatalog.cpp
lib/src/components/SettingsMenu.cpp
lib/src/components/SplashScreen.cpp
lib/src/core/AssetProvider.cpp
lib/src/core/Engine.cpp
lib/src/core/FileSystem.cpp
lib/src/core/GameLauncher.cpp
lib/src/core/Launcher.cpp
lib/src/core/PresentedFrame.cpp
lib/src/core/VisualTransition.cpp
lib/src/debug/ImGuiTheme.cpp
lib/src/debug/ImGuiTheme.hpp
lib/src/localization/Encoding.cpp
lib/src/mcp/MCPServer.cpp
lib/src/media/AudioMixer.cpp
lib/src/media/MediaDecoder.cpp
lib/src/media/MediaPlayer.cpp
lib/src/media/PcmPlayer.cpp
lib/src/media/TextureAlpha.cpp
lib/src/mobile/AndroidDisplay.cpp
lib/tests/AssetProviderTests.cpp
lib/tests/AudioBufferTests.cpp
lib/tests/CMakeLists.txt
lib/tests/DisplayModeCatalogTests.cpp
lib/tests/InfrastructureTests.cpp
lib/tests/MediaAnalysisTests.cpp
lib/tests/MediaPlayerRateTests.cpp
modules/providers/gpk/include/Kotonoha/SchoolDaysAssetMount.hpp
modules/providers/gpk/src/SchoolDaysAssetMount.cpp
vcpkg.json
```

## Apendice B: manifesto SHA-256 dos GPKs da origem

Os 29 GPKs retail do destino foram comparados com estas linhas. `LxH.GPK` veio dos pre-requisitos, nao dos 29 retail.

```text
Nome | Bytes | SHA-256
BGM.GPK | 74822965 | 1B14369910D4C348BF2B8862578A35CCAA22290F0D9A4878DD6C4FD0B193F093
Event00.GPK | 73344314 | 86FDFBF480318FA42DF48982169BA4FC9081B5C3105E2811535A767FEAFC776D
Event01.GPK | 589092020 | 298CB596CB2D399E1AD337FA4E7E087178FE8BE828AB9B39190EF92DFC132748
Event02.GPK | 449769467 | 892203E733E84C41F957EB1FA4BB789CAF52FAA7C9FC26975298AFF0FE76F477
Event03.GPK | 1353519287 | 8582FC9C19D6316CEA7CDBF78D88A3DE05E788A87A777EEC6711BA3681A0CC02
Event04.GPK | 1657014466 | E6832DD9CE07C22422A8ED10EF1D0EA18E59A41180CB8EAA1CDE95890854B794
Event05.GPK | 1414719858 | EF70932E483B579E3DBE94BE81F81748098E6DB6FD12E34CC4C55EB5FF63E4C2
Ini.GPK | 9168 | 61FB0202C97818B29DFE00E65893B0F419EE73D85C08077267F2F16ED09B30E0
LxH.GPK | 237329938 | 772B501FC70AE89E52861378A99FF1B27D3C47EB20B2473836591069143EDB64
Movie00.GPK | 207557810 | 9C372FF7B9431AD63E8BB82EEA859DD5F4E984EF7BB8C2D67F48B233E0635EC9
Movie01.GPK | 173181787 | B8A7E00846B203BB03A2832872A593D8BADDC0E8C8644B20AC3D2E71C6AC1879
Movie02.GPK | 390890144 | 2E1AD96BBDB236D3AF471D31B3F9D87B42B11ED9E3898901338977FE2FC20DBD
Movie03.GPK | 961383204 | 1B312D3A232A1B18594ED338F1EA6B5891D7DFFD89C71FC53E6D69DEE68BE15C
Movie04.GPK | 1183457158 | 0E0128E5BB477D8CCD05D1FF8766491159425B3113A400663EE86BF1343FBD1F
Movie05.GPK | 1644748474 | 8E1B9AA4077723941EB22D548BF51CA0D1A056C508E8363D787E39F12F64F59B
Script.GPK | 1699276 | E82DC824449381B9A132AA092740C4169EB3D77A820D969046B74CA1D9DF4ADF
Se00.GPK | 24980841 | 834FFC64F942FD13BA083313EC1EB14039C97443AD695C3CF274E808DAE4D8D7
Se01.GPK | 143028126 | 9674AD4E937E63E7984F6FEDDA8AAF61E033008B17DFEE21A573D9648D3A32CD
Se02.GPK | 128836109 | B4AA7EDCA4238EA0D13F2F268A389CBA44F9C8B45D81D8389E64453953654D6E
Se03.GPK | 346828069 | 2162810756BE3AF1F03D2FFEACC6105D330FC2867E6A9861E0544F8DC03B4691
Se04.GPK | 47661995 | 169ED288A655C5F4B7755DA373F5DA4F997C40D57B62BE03E59A0FF1F83ADA21
Se05.GPK | 22356596 | 2E272EF2C0D846359A7F688821896E20517E0015828C7D6704A7E7AD172D2D0D
SysSe.GPK | 23714806 | E00BB560A016295E0774ED55E7375963803AABFAAD299CD48A99E53BAB95097A
System.gpk | 326138398 | 915A631410CA081327FC7C11B8AA0C54A7E93EAEFD9F6EACCC7DAC9469E6F54C
Voice00.GPK | 14167471 | 98A680AF3DF9686903F61D3ABF4765611CC8A378408A0635B54E17D72B614876
Voice01.GPK | 47157862 | 66939DA0BBBB31403923B2A80E2F1251CA8F33AC000F664C152F83DFFAAC7D85
Voice02.GPK | 54098720 | 86CD2DB4947E5D7EB5A51ECA41E7BEA59A01F1796E699B5AD52EBA93B11023A1
Voice03.GPK | 171658535 | 00C2C94EBB5FF24EAD8A836A3852B809A3F427D695957BCC77F1324CFD267845
Voice04.GPK | 212900377 | 55BBD990CA4859C3DCAE3534EEB6EB17F0C6079179204DF2432AB74627127020
Voice05.GPK | 219448508 | 9F7DF70B852904532E9CAFFF173A4881CB27FCEF450C280C1F12ECE90D8C5CEA
```
