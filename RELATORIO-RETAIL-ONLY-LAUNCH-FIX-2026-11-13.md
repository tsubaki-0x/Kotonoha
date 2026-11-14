# School Days HQ retail-only com dublagem — corrigido Play que fechava o launcher

## Diagnostico

Falha reproduzida em build `KOTONOHA_SCHOOL_DAYS_RETAIL_ONLY=ON` usando perfil PT-BR copiado para QA: a validacao de `ConfigureSchoolDaysTranslatedGpks` recusava `Script - Brazilian.GPK` sem `pt-BR-lxh.GPK` mesmo que o resto do codigo retail-only ja tivesse desativado os epilogos extras. O mesmo requisito residual existia em espanhol (`es-lxh.GPK`). Quando a excecao ocorria apos o clique em Play, `SDL_AppIterate` apenas escrevia em stderr; o executavel Windows GUI fechava sem avisar o usuario.

## Correcao

- `SchoolDaysAssetMount.cpp`: para `KOTONOHA_SCHOOL_DAYS_RETAIL_ONLY`, valida PT-BR e espanhol apenas pelo nome correto do `Script.GPK` base, sem exigir GPK de epilogo; a validacao estrita de LxH nas builds integradas permanece intacta.
- `main.cpp`: erros de startup apos o launcher passam a exibir uma mensagem de erro Windows visivel. A diferenca DEV/DIST no nivel dos logs continua a mesma.
- `SchoolDaysRetailOnlyGpkProfileTest.cpp`: teste sintetico de tres selecoes de idioma nativas (EN/PT-BR/ES) sem LxH, e rejeicao de escolhas incompatíveis.

## Validacao

- Release Windows x64 recompilada com `KOTONOHA_DISTRIBUTION_BUILD=ON`, `KOTONOHA_ENABLE_PLAYER_DUBBING=ON`, `KOTONOHA_SCHOOL_DAYS_RETAIL_ONLY=ON`.
- `ctest -R ^SchoolDays\\.RetailOnlyGpkProfile$`: passou.
- Inicializacao via `--no-launcher --school-days` em QA isolado, com exatamente **29 GPKs** e sem `LxH.GPK`, passou nos cinco perfis: ingles retail, ingles GPK, pt-BR GPK, espanhol GPK e espanhol JSON. Todos abriram `School Days HQ` e nao escreveram erros em stderr; os processos foram encerrados intencionalmente pelo teste.
- Com o renderizador OpenGL salvo no `settings.json` do jogador (copiado para QA), ingles original e PT-BR GPK tambem inicializaram com janela `School Days HQ` e stderr vazio.
- Nao houve verificacao manual de F10/microfone nem Play por clique dentro do launcher nesta rodada. A nova mensagem de erro permite identificar qualquer falha remanescente.
- Nenhuma instalacao, save ou distribuicao antiga foi sobrescrita. Os arquivos de idioma usados na QA foram copiados para `build/school-days-retail-only-playfail-qa-20261113`.

## Entrega

- Pasta: `dist/SchoolDaysHQ-Windows-Virgem-Dublagem-Sem-LxH-CORRIGIDA-2026-11-13`
- ZIP: `dist/SchoolDaysHQ-Windows-Virgem-Dublagem-Sem-LxH-CORRIGIDA-2026-11-13.zip`
- Entrega virgem: nenhum pack, save, configuracao pessoal ou traducao LxH.
- Executavel da pasta especial anterior `dist/SchoolDaysHQ-Windows-Virgem-Dublagem-Sem-LxH-2026-11-13/SchoolDays.exe` substituido pela correcao **apos backup e verificacao de hash**. Backup: `SchoolDays.exe.before-launch-fix-20261113.bak`. O perfil, Config.DAT, saves e 29 GPKs dessa pasta nao foram modificados; o ZIP original nao foi reescrito.
- SHA256 do EXE corrigido: `DEC8CF475F82A063145D34D6E29153BEC72BAAF699AB8381C9B4E3917130A579`.
- SHA256 do ZIP novo: `D2C484682195032AD40E96AD15D3A2134F8B885F4DC1634A9E8D4C271653A202`.
- Branch: `feature/retail-only-dubbing-20261113`, commit de correcao local, nenhum push.
