# Auditoria e correcao zh-CN / ru — School Days HQ retail-only com dublagem

## Origem do problema

Em `ConfigureSchoolDaysTranslatedGpks`, a variante sem LxH rejeitava qualquer `zh-CN` antes de validar o arquivo `Script - chinese-compatible.GPK` (o erro mostrado no launcher). Alem disso, `ru` nao existia na allow-list do perfil GPK, no enum de idioma ou no launcher. Os quatro GPKs traduzidos haviam sido copiados para `translations/scripts`, mas copiar um arquivo nao equivale a integrar seu idioma.

## Auditoria real dos GPKs

Referencia retail usada SOMENTE para leitura: `D:\SCHOOL DAYS HQ\Overflow\SCHOOLDAYS HQ\Packs\Script.GPK`, SHA256 `E82DC824449381B9A132AA092740C4169EB3D77A820D969046B74CA1D9DF4ADF`.

**Chines compatibilizado:** `Script - chinese-compatible.GPK`, SHA256 `355BF7EAD91F41F4F768F3392E18E5650BB22569A456BFA00A8C5F96CE401514`, 6.006.448 bytes. Possui 1.859 entradas PIDX, exatamente os mesmos nomes/estrutura da versao retail. Das 1.857 ORS, todas mantem comandos/tempos/recursos segundo auditoria offline, incluindo os mesmos tres comandos malformados do retail (ignorados pelo parser C++ dos dois lados). 30.484 de 30.485 linhas PrintText foram alteradas e 287 eventos de escolha foram traduzidos. ORS em UTF-8 valido. **Nao e o GPK chines antigo com GB18030/quoted comma**, que continua rejeitado por basename e por validacao nativa.

**Russo original:** `Script - Russian.GPK`, SHA256 `1F67DDA936FFEC15B5543B1578B2C79B6666371A21765273F864A5C6EABA8394`, 1.856.544 bytes. Possui 1.859 entradas PIDX identicas ao retail. Em 1.856/1.857 cenas todas as sequencias de eventos/ticks batem; a cena `English/03/03-KB-D10.ENG.ORS` possui UMA fala com campo TAB malformado no retail (descartada pelo parser), enquanto no russo original o campo TAB havia sido normalizado e se tornava um evento EXTRA.

**Russo corrigido:** ferramenta de QA `games/school-days/tools/normalize_russian_retail_script_gpk.py` preserva TODOS os 1.859 membros e textos e reinsere um TAB de compatibilidade naquela linha, para reproduzir o comportamento do retail. Produz novo arquivo, nunca sobrescreve o original. Saida gerada em `tools/qa-lab/state/russian-retail-normalized/run-ba706592a29444cd8a0b9702ddcd7233/Script - Russian.GPK`, SHA256 `1782F2FEC1A679B7A3D6AC5C1D804D5A5FA24F8893778F31555EF5EE1D61629E`. A unica fala segue no arquivo traduzido mas, como no retail, o parser nao a exibe. Nao houve fallback automatico para ingles. Algumas frases da comunidade ainda podem precisar de revisao linguistica; UTF-8 e alinhamento de eventos nao certificam traducao perfeita. Nenhum credito `Juan Ochoa (JNC)- LxH .2021` encontrado nos ORS russos auditados.

## Codigo atualizado

- `SchoolDaysGpkLanguageProfile.hpp`: habilita `ru` apenas em `KOTONOHA_SCHOOL_DAYS_RETAIL_ONLY`; caminho canonico `Script - Russian.GPK`.
- `SchoolDaysAssetMount.cpp`: admite `zh-CN` SOMENTE na variante retail-only, exige o nome `Script - chinese-compatible.GPK`, valida toda a estrutura GPK no mount; `ru` exige o nome correto e 1.859 entradas + paridade ORS. Builds com LxH mantem seu bloqueio antigo, evitando regressao.
- `SchoolDaysLauncher.{hpp,cpp}`: novo idioma russo no combobox, persistencia e recuperacao da selecao; escolha chinesa aponta somente para arquivo compatibilizado; sem exigir traducao LxH.
- `SchoolDaysScenes.cpp`: as escolhas renderizadas passam por `SchoolDaysDisplayNarrativeMarkup::Project` na variante retail-only, removendo `^` terminal sem alterar dados GPK, tempos nem decisoes. Importante para as 293 escolhas russas terminadas em `^`.
- Testes de perfil GPK e de marcador de escolhas atualizados para EN/PT-BR/ES/ZH-CN/RU.

## Validacao

- CMake Release `build/school-days-retail-only-dubbing-dist`: compilacao sem erro de SchoolDays e dos testes.
- Flags confirmadas: `KOTONOHA_DISTRIBUTION_BUILD=ON`, `KOTONOHA_ENABLE_PLAYER_DUBBING=ON`, `KOTONOHA_SCHOOL_DAYS_RETAIL_ONLY=ON`.
- `SchoolDays.GpkLanguageProfile` + `SchoolDays.RetailOnlyGpkProfile`: 2/2 testes passaram, incluindo marcadores russos e chineses nas escolhas.
- Inicializacao completa com `--no-launcher`, 29 GPKs retail copiados por hardlink de QA, nenhum `LxH.GPK`: en/pt-BR/es/zh-CN/ru abriram janela `School Days HQ` e tinham stderr vazio; 5/5.
- Inicializacao **OpenGL** com configuracoes graficas da QA em `zh-CN` e `ru`: 2/2 processos apresentaram janela `School Days HQ` e stderr vazio.
- Experiencia de GUI clicando em Play, texto chinês durante cenas, cena russa anomala e gravacao F10 **nao** foram testadas visualmente. Os testes de inicializacao usam processos isolados terminados intencionalmente.
- A suite antiga `SchoolDays.DisplayNarrativeMarkup` ficou bloqueada e foi encerrada; os dois testes de perfil citados acima passaram e incluem as verificacoes de caret. Portanto nao afirmar que a suite completa passou nesta rodada.
- `C:\Windows\Fonts\msyh.ttc` existe na maquina de QA; isso nao comprova fonte disponivel em outras maquinas.

## Distribuicao

`dist/SchoolDaysHQ-Windows-Virgem-Dublagem-Sem-LxH-ZH-RU-2026-11-14`

`dist/SchoolDaysHQ-Windows-Virgem-Dublagem-Sem-LxH-ZH-RU-2026-11-14.zip`

ZIP de 25,54 MB, SHA256 `98495DC47C01942C444F00B0D081A528989B725CC48E78F1FC54C2789E8D6013`. EXE SHA256 `E3884E794E442B547AC4DDA94B8606369CBC0B0AB2A52C38B60E26457AD60A22`.

Nova distribuicao completamente virgem: `Packs` vazia, sem saves/settings.json/Config.DAT/translation-pack.txt, sem GPK/JSON LxH. Somente GPKs de traducao da historia base estao em `translations/scripts`. A pasta `Packs` original e as distribuicoes anteriores nao foram alteradas.

Tambem foi atualizado **somente** o `SchoolDays.exe` e o `translations/scripts/Script - Russian.GPK` na pasta que o usuario ja havia configurado: `dist/SchoolDaysHQ-Windows-Virgem-Dublagem-Sem-LxH-CORRIGIDA-2026-11-13`. Antes da substituicao, os dois hashes originais foram verificados e os arquivos foram preservados como `SchoolDays.exe.before-zh-ru-fix-20261114.bak` e `translations/scripts/Script - Russian.GPK.before-zh-ru-fix-20261114.bak`. Os 29 GPKs de `Packs`, `settings.json` e `Config.DAT` permaneceram intactos. O ZIP antigo nao foi reescrito, e a distribuicao com LxH nao foi tocada.

Nao houve git push. Codigo/relatorio registrados somente em branch local.
