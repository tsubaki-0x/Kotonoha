# Correcao de LOAD dos finais LxH - School Days HQ / Kotonoha

Data: 13/11/2026. Branch local: `sync-upstream-2026-11-13`. Commit: `ea250cb` (`Fix LxH ending loads with native GPK localization and regression tests`). **Nenhum push foi executado.**

## Problema e causa no codigo

Sintoma relatado: ao selecionar saves que carregam finais LxH, a versao de distribuicao retornava ao menu. A DEV oferece mensagens de diagnostico, mas a DIST oculta os logs internos de informacao. A hipotese inicial de "node" nao foi confirmada como causa.

O fluxo da versao anterior montava ORS traduzidos do `Script.GPK` nativo e de overrides LxH, mas ainda tentava aplicar `SchoolDaysTranslationGate::TranslateLayered` aos textos/subtitulos ja presentes nesses GPKs durante `bindScene` e `Subtitles::bind`. Essa validacao exige cobertura JSON e pode interromper a abertura da cena quando se escolhe um idioma GPK sem JSON correspondente. O runtime de save executa `bridge.LoadSave` e abre a cena a seguir; por isso a traducao indevida nesse ponto era consistente com a falha observada. Os testes verificaram `Next`, `END_ROLL`, filme e recursos da timeline, sem encontrar ausencia de arquivos LxH nos perfis certificados.

## Correcao efetuada

- `games/school-days/src/main.cpp` marca explicitamente a selecao de `Script.GPK` / LxH GPK nativos com `nativeGpk=true`.
- `SchoolDaysRuntime.{hpp,cpp}` conserva o sinalizador, evita segunda passagem por tradutor JSON para textos nativos e continua projetando a marcacao/supressao de credito LxH na camada de exibicao.
- `SchoolDaysScenes.{hpp,cpp}` evita retraduzir cues nativas, preservando speaker, timing e texto do ORS. **O modo JSON continua com validacao estrita**, sem fallback silencioso para ingles.
- `SchoolDaysHandoffRuntimeTest`, `SchoolDaysLxhLocaleTest` e `SchoolDaysLxhRetailSaveProbe` cobrem carregamentos reais copiados, transicao de LxH, timeline e modo GPK nativo.

Nao foram modificados arquivos retail, saves originais, mapas de cenas ou logica de countdown. O merge anterior e os recursos da dublagem foram preservados.

## Validacao

| Verificacao | Resultado |
| --- | --- |
| Compilacao DEV / Release | Sucesso em `build/school-days-l4-two-dll-staticmd` |
| Compilacao DIST / Release | Sucesso em `build/school-days-l4-dubbing-dist-staticmd` |
| Suite `stress_l4.py` executada pelo Codex antes do limite | **74/74 testes + 4/4 smokes** com software renderer; `GPK_UNCHANGED=True` em `build/school-days-l4-two-dll-staticmd/qa-stress/stress-20261113-221706/stress-report.json` |
| `SchoolDaysLxhRetailSaveProbe` com dados isolados | **7/7 saves** em cada idioma (`pt-BR`, `en`, `es`), ou 21 execucoes, com rotas, `Next`, filme e END_ROLL validados |
| `SchoolDaysHandoffRuntimeTest` com dados isolados | **Passou nos 3 idiomas**: seis saves reais dos endings LxH, um save retail de outra cena e um save LxH adicional gerado para `05/05-A2-F00`; gameplay ativo, timeline com Next e fade habilitado nos endings LxH |
| Inicializacao DEV software | `Engine started (software renderer)` e termino controlado por `--frames`; saida em `build/lxh-save-qa-continue-20261113/startup-DEV/` |
| Inicializacao DIST software | Processo permaneceu ativo apos 6 segundos, encerrado propositalmente pelo teste; stderr silencioso como esperado na DIST |
| Dependencias basicas | EXEs executam `--help` com codigo zero, DLLs `SDL3.dll` e `SDL3_ttf.dll` presentes; nao comprovada portabilidade em outro computador |
| Packs | 30 GPKs originais presentes em cada pacote, nomes/tamanhos iguais aos da pasta de origem; hashes de `Script.GPK` e `LxH.GPK` identicos entre DEV e DIST |
| Teste final nos pacotes entregues | `SchoolDaysLxhRetailSaveProbe` **7/7** e `SchoolDaysHandoffRuntimeTest` **8/8** em `pt-BR` nas pastas DEV e DIST, executados novamente apos a copia dos recursos. Logs em `build/lxh-save-qa-continue-20261113/final-*.log`. |

Observacao: uma repeticao opcional da CTest com filtro acionou o `SchoolDaysPortTest` por dependencia de fixture; a repeticao foi interrompida por demorar mais do que o esperado. A **suite completa anterior, executada apos compilar o patch, havia passado 74/74**. O teste direto do runtime com saves foi executado em seguida e passou em `pt-BR`, `en` e `es`.

## Pacotes entregues

**DEV:** `D:\Dev\Kotonoha-v3-fonte\dist\SchoolDaysHQ-DEV-LxH-Fix-2026-11-13`

- `SchoolDays.exe` SHA-256: `A366D5C4BC3455A63AD104D73466E235CE5808647419E3544FBF107140A2DD7B`
- Build diagnostica (`KOTONOHA_DISTRIBUTION_BUILD=OFF`); mensagens internas disponiveis.

**DIST:** `D:\Dev\Kotonoha-v3-fonte\dist\SchoolDaysHQ-DIST-LxH-Fix-2026-11-13`

- `SchoolDays.exe` SHA-256: `6ED44D113B6ACFACAAEEA3A5D5B76DAEE64D00D7AA1B6D59536195464FC6215C`
- Build Release (`KOTONOHA_DISTRIBUTION_BUILD=ON`, `KOTONOHA_ENABLE_PLAYER_DUBBING=ON`); logs internos reduzidos.

Hash `Script.GPK`: `E82DC824449381B9A132AA092740C4169EB3D77A820D969046B74CA1D9DF4ADF`.

Hash `LxH.GPK`: `772B501FC70AE89E52861378A99FF1B27D3C47EB20B2473836591069143EDB64`.

Os pacotes novos sao pastas independentes com executaveis, DLLs e recursos; a antiga distribuicao `D:\Dev\Kotonoha-v3\dist\SchoolDaysHQ-Windows-Clean-Distribution-v3` e a distribuicao de merge `dist\Kotonoha-2026-11-13` nao foram substituidas. Nenhum save original foi copiado para as novas pastas. Os testes usaram dados separados sob `build\lxh-save-qa-continue-20261113`.

## Pendencias / riscos

- **Nao houve teste manual clicando em LOAD na interface**; o carregamento foi validado pelo bridge/runtime, sem reproduzir visualmente cada epilogo completo. Confirmar video WMV, audio OGG, tela PNG e handoff ao countdown de 30 s exige QA dentro do jogo.
- A sessao de dublagem esta habilitada na DIST, mas microfone, gravacao, preview e controles nao foram testados manualmente. Por desenho atual do CMake, o Player Dubbing so e incorporado quando `KOTONOHA_DISTRIBUTION_BUILD` e `KOTONOHA_ENABLE_PLAYER_DUBBING` estao ambos ON; a DEV anterior nao habilita esse modulo.
- O motor usa MSVC dinamico (`/MD`); em outra maquina pode ser necessario instalar Microsoft Visual C++ Redistributable x64.
- Os GPKs nao devem ser enviados ao GitHub. Os pacotes nao estao zipados para nao duplicar dezenas de gigabytes de arquivos compactados.
