# School Days HQ v1.02 / Kotonoha L4 — entrega Windows de duas DLLs

**Estado 07/11/2026 (launcher HD, chinês EN/ES/zh-CN e General FULL):** build
Windows L4 atualizada e visualmente testada no launcher. A validação do
fullscreen exclusivo e dos finais ainda depende de QA físico próprio.

## Executável publicado

- Pasta: `build/school-days-dev-control4/games/school-days/Release`.
- Arquivos binários locais: `SchoolDays.exe`, **`SDL3.dll`** e
  **`SDL3_ttf.dll`**. Nenhuma outra DLL local.
- EXE SHA256:
  `9db3ddf3995b199e810e8d64cb12f720edb1c388b10e1386e5fc2379c9c241ec`.
- O EXE ainda depende dos componentes do Windows e do Microsoft Visual C++
  Runtime x64. Duas DLLs significa **duas DLLs distribuídas localmente**,
  não a ausência de bibliotecas do sistema operacional.
- Versão anterior: `build/school-days-dev-control4/backup-old-runtime-dlls-20261107`
  (EXE antigo e 29 DLLs, não apagar).
- Backup imediatamente anterior à atualização de launcher/display:
  `build/school-days-dev-control4/backup-l4-launcher-display-20261107-1758`
  (EXE anterior e duas DLLs, não apagar).
- Backup imediatamente anterior à correção de General FULL e feedback:
  `build/school-days-dev-control4/backup-l4-window-full-sync-20261107-1815`
  (EXE e duas DLLs anteriores, não apagar).
- Backup da versão imediatamente anterior à integração chinesa:
  `build/school-days-dev-control4/backup-l4-chinese-locale-20261107-191940`
  (EXE/DLLs anteriores, mais cópias dos JSONs EN LxH e zh-CN anteriores).
- Último backup de binários, anterior à correção de placeholder invisível:
  `build/school-days-dev-control4/backup-l4-chinese-blank-render-20261107-192547`.
- Backup anterior à atualização visual HD:
  `build/school-days-dev-control4/backup-l4-hd-launcher-final-20261107-204605`.

## Launcher HD final e instalação local limpa

- Único `SchoolDays.exe` inicia o launcher com PLAY/OPTIONS/QUIT. A janela
  Options oferece abas Display, Graphics, Audio, General, Language e Advanced;
  `--no-launcher` permanece disponível para testes/headless.
- As duas únicas artes usadas são as imagens autorizadas pelo usuário:
  ícone preto e branco incorporado via recurso Windows (8 resoluções) e
  Kotonoha/Sekai com a logo School Days HQ. Nenhuma imagem de referência
  do Spider-Man integra o produto.
- Arte de melhor qualidade substituída sem recompressão:
  `C:\Users\Suzu\Downloads\homebg_waifu2x_art_scan_noise2.png` (950×600),
  SHA256 `3d2586474a2fc4e163eca33cd89e4e6ca5faa9d97fab98d37fb07c27f01d82fe`.
  Idêntica byte a byte em `launcher-art/`, na build isolada, na Release L4 e
  no staging local. Screenshot da tela principal confirmado visualmente.
- Compilação Release ALL_BUILD serial PASS; **CTest 68/68 PASS (40,12 s)**.
  Publicação com backup verificada: 89 arquivos de saves/configuração/assets/
  tradução/display permaneceram com SHA256 idênticos; 30 GPKs e duas DLLs.
- Staging de instalação criado localmente em
  `build/school-days-dev-control4/School Days HQ - Clean Local L4`, com 121
  arquivos copiados e três arquivos auxiliares. Os 30 GPKs são cópias reais
  (não junction), sem saves/configurações pessoais. Manifesto SHA256:
  `3d7f0df94b8938d6b781740eef1400d77f5dded5d8aa4aee5f5fb6cc1e6c4fad`.
  **Primeiro boot sem `--launcher` e a partir de CWD externo: PASS**; imagem
  HD exibida e sem configuração/save gerado na pasta limpa. Os packs são
  comerciais e o staging é somente para uso local privado.

## Conteúdo e configuração

- `Packs/`: 29 GPKs comerciais originais intocados + `LxH.GPK` obrigatório;
  30 packs ao todo. `LxH.GPK` tem 28 recursos e SHA256
  `772b501fc70ae89e52861378a99ff1b27d3c47eb20b2473836591069143edb64`.
- `display-options/Script/English` era um diretório **vazio**, resíduo
  da antiga distribuição de epílogos soltos. Foi removido. Também não há
  `display-options/LxH`. Os sete ORS grafted e 21 mídias/cartões agora
  vêm exclusivamente do pacote. `display-options/System/Option` e a
  arte do usuário permanecem.
- Padrão inicial inglês: `settings.json` com `translationPath=""` e
  `language.txt` com `english`. Base EN do `Script.GPK`, falas L×H
  de `translations/en-lxh.json`. Para espanhol: abrir o launcher
  existente com `SchoolDays.exe --launcher`, selecionar **Spanish (JSON)**;
  HQ usa `translations/es.json`, epílogos `translations/es-lxh.json`.
  Os sete cartões PNG continuam visualmente em inglês universal.
- Saves comerciais legítimos (sem sidecar L×H legado) já estavam em `Save/`:

| SaveFile | Final original com epílogo |
| --- | --- |
| `SaveFile000.DAT` | `05-A2-Z00` |
| `SaveFile001.DAT` | `05-A2-F00` |
| `SaveFile002.DAT` | `05-G0-L03` |
| `SaveFile0012.DAT` | `05-KC-F00` |
| `SaveFile0013.DAT` | `05-SC-L03` |
| `SaveFile0017.DAT` | `05-SB-E01` |
| `SaveFile0019.DAT` | `05-B0-F00` |

  SLog válido e `GlobalFlag.DAT` coerente para todos os sete; verificações
  por hash antes/depois da publicação. Backup com manifesto:
  `build/school-days-dev-control4/backup-lxh-seven-saves-20261107`.
  A correspondência de saves é estática; a execução do epílogo até o
  retorno ao título **não foi certificada fisicamente** nesta entrega.

## Evidências de build

- Build isolado:
  `build/school-days-l4-two-dll-staticmd`, MSVC 14.51,
  vcpkg triplet `x64-windows-static-md`, `KOTONOHA_STATIC_ENGINE=ON`,
  `KOTONOHA_STATIC_IMGUI=ON`, `KOTONOHA_DEV_CONTROL=OFF`.
- **ALL_BUILD Release PASS** na entrega anterior; atualização de
  launcher/display recompilada duas vezes em Release na mesma build
  estática isolada, ambas **exit 0**. **CTest atualizado 68/68 PASS**,
  repetido após os últimos ajustes (**36,25 s** na segunda rodada).
- Após o ajuste final de General FULL e estado visual, rebuild Release
  **serial PASS** (uma tentativa `--parallel 4` teve contenção de arquivo
  de `vcpkg z-applocal`, sem erro de C++); **CTest 68/68 PASS (36,64 s)**,
  incluindo ConfigMenuDisplay, DisplayPolicy, RuntimeConfigStorage e
  Launcher. O backend SDL dummy não expõe modos exclusivos: o contrato
  Exclusive é validado deterministicamente e a operação SDL real não foi
  executada contra o monitor físico nesta etapa.
- `dumpbin /dependents`: SchoolDays.exe importa localmente somente
  SDL3 e SDL3_ttf; bibliotecas de mídia, fontes, engine e ImGui linkadas
  estaticamente. Nenhum carregamento de DLLs de `%TEMP%` necessário.
- Probe do GPK real na entrega anterior: PASS sete ORS estendidos e
  21 PNG, WMV, OGG seekable. Smokes headless da **build candidata
  idêntica à publicação**, EN e ES, `--frames 5`, `--no-config`,
  `--no-launcher`, renderer software e SDL dummy: **exit 0** em ambos,
  usando Packs da L4 e perfis `--documents` isolados:
  `D:/Dev/_temp_lxh_static_audit_worker8/qa-l4-launcher-display-smoke-20261107-1758`.
- Publicação com `publish_two_dll_l4.ps1` **dry-run PASS** e depois
  `TWO_DLL_RELEASE_PUBLISHED`. EXE e duas DLLs idênticos por SHA256
  à candidata. Manifesto de **59 arquivos** de packs, saves,
  traduções, arte e configuração preservado, **0 divergências**;
  30 GPKs e duas DLLs locais após a cópia.
- Nova publicação após a correção: smokes EN/ES da candidata **exit 0**,
  `--frames 5`, `--no-config`, `--no-launcher`, `--no-asset-overlay`,
  SDL dummy e software renderer, com `--documents` privados em
  `D:/Dev/_temp_lxh_static_audit_worker8/qa-l4-window-full-sync-smoke-20261107-1815`.
  `publish_two_dll_l4.ps1` dry-run e deploy **PASS**. Manifesto de
  **60 arquivos** de dados/configurações manteve nomes, tamanhos, datas
  e hashes aplicáveis sem diferenças; 30 GPKs e precisamente as duas
  DLLs locais continuam presentes. Somente EXE e DLLs foram publicados.
- Sem GUI física nesta etapa. QA real de sete finais × EN/ES ficará a
  cargo da comunidade, que poderá relatar bugs futuros.

## Idiomas EN/ES/zh-CN e assinatura dos epílogos

- O launcher mantém English/Spanish e acrescenta **Chinese - Simplified**,
  selecionando `translations/zh-CN.json` e automaticamente o arquivo
  correspondente `zh-CN-lxh.json`. Também permite escolher manualmente
  o JSON base e o JSON LxH separadamente, voltar ao pareamento automático
  e salva a seleção em `language.txt` junto ao `settings.json` existente;
  reinício direto e por launcher usam a mesma opção. O vínculo manual é
  invalidado ao trocar o JSON base para não misturar idiomas por acidente.
- O JSON chinês base original tinha 590 referências `@ors` deslocadas por
  um evento. Reindexação precisa preservou **todos** os valores traduzidos,
  chaves literais e a exceção do NODE 1334. Novo `zh-CN.json` SHA256
  `040010c4dcbf5961f6e53c122870790c7064cd2acc8d49473c9eee292b8d2a23`.
  Traduções validadas contra script original e LxH: 30.968/30.968 HQ
  (485 choices) e 273/273 epílogos, zero entradas faltantes, em branco,
  identidades sem revisão ou conflitos entre base e sidecar.
- Requisito do usuário: `Juan Ochoa (JNC)- LxH .2021^` **somente espanhol**.
  As sete ocorrências em valores de `en-lxh.json` foram substituídas por
  marcador invisível U+200B, preservando as 273 chaves de contexto.
  Novo SHA256 `fecf3937ef8d3c3a51b5172c3b5a864fcc12f5597adf75d62ebc603a98e03141`.
  `zh-CN.json` e `zh-CN-lxh.json` não continham a assinatura; `es.json`,
  `es-lxh.json` e manifestos ES permanecem inalterados. A linha-fonte no
  GPK comercial não foi editada; a projeção de exibição a oculta em EN/zh-CN
  também nas legendas e BackLog/PlayData, mantendo-a apenas no espanhol.
  Como SDL_ttf não aceita rasterizar uma linha contendo apenas U+200B,
  a projeção de apresentação transforma **somente** esse placeholder isolado
  em linha vazia depois da validação dos JSONs; caracteres U+200B embutidos
  em texto normal são preservados. Testes cobrem os dois casos.
- NotoSansJP-Regular.ttf instalado não cobre todos os 2.218 caracteres
  distintos do JSON base chinês (515 faltantes) nem 90 dos 455 do LxH.
  No Windows testado, a Microsoft YaHei (`msyh.ttc`) cobre ambos integralmente;
  choices e BackLog usam fallback SDL_ttf apenas nos glifos ausentes.
  A renderização ASS do libass também passou no teste de glifos chinês.
  Nenhuma fonte proprietária foi empacotada; outras máquinas sem YaHei
  precisam de um fallback CJK instalado e QA visual adicional.
- Build estática serial **exit 0**, CTest **68/68 PASS (41,59 s)**, incluindo
  Launcher, LxH Locale, Display Narrative Markup e políticas anteriores.
  Smokes headless EN/ES/zh-CN tanto na candidata quanto após a publicação
  finalizaram **exit 0**. Novo manifesto de 62 arquivos: apenas os dois JSONs
  indicados mudaram; 30 GPKs, saves, configurações, espanhol e outras
  traduções mantiveram integridade. DLLs locais continuam exatamente duas.
  Sem execução física do game/launcher nem commit ou push nesta etapa.
- Após a última correção de renderização de U+200B, build serial e suíte
  completa repetidas, **68/68 PASS (41,27 s)**. Binários publicados de novo
  com `publish_two_dll_l4.ps1` (dry-run + deploy PASS), SHA do EXE acima,
  sem substituir novamente traduções, packs, saves ou configurações.

## Atualização funcional de Display Options e launcher

### Correção posterior: feedback imediato, FULL exclusivo e sincronia

- O estado visual de Display Options era cacheado e não repassado ao
  presenter após Apply: um clique em Resizable/VSync/escala/default podia
  mudar o valor internamente e só parecer atualizado após trocar de aba.
  Agora o estado selecionado é recalculado após Apply, inclusive quando
  chamado automaticamente; o hover não encobre o CHIP selecionado.
- Confirmar um caminho no editor de GLSL passa por Apply e grava
  também o sidecar na hora, evitando perda ao sair da aba.
- General Options > Window Size > FULL agora seleciona **Exclusive**
  (`SDL_SetWindowFullscreenMode` + `SDL_SetWindowFullscreen` pela engine).
  Preserva resolução/refresh manuais que vieram do launcher ou de
  Display Options; WINDOW mantém a janela decorada e maximizada e não
  apaga o último fullscreen escolhido. Sem QA visual físico nesta etapa.
- O launcher e Display Options usam o mesmo `settings.json` do
  executável: uma escolha manual de qualquer lado é reaplicada na
  próxima inicialização e nunca substituída pela detecção automática
  se houver settings válidos. O launcher também relê `Config.DAT` e
  `Kotonoha.ini` depois que o jogo fecha/flush; isso não é sincronização
  instantânea de dois processos abertos ao mesmo tempo. Testes sintéticos
  comprovam launcher -> persistência e a reabertura depois de alterações
  físicas/áudio/aspect feitas pelo jogo. Em modo Borderless, o rótulo
  mostra a resolução efetiva do desktop em vez do tuple Exclusive antigo.
- A gravação de JSON/DAT/INI/sidecars é compartilhada, mas não é uma
  transação atômica entre arquivos; falhas de I/O ainda exigem atenção.

- Resolução escolhida no menu é aplicada imediatamente ao engine SDL
  (sem exigir o antigo botão Apply). Uma seleção diferente do desktop
  enquanto Borderless promove a Exclusive, em vez de bloquear
  silenciosamente a troca. Windowed altera o tamanho da janela;
  fullscreen conserva resolução e refresh configurados. A mudança
  física não foi comprovada visualmente por decisão de não abrir a GUI.
- Detecção de monitor e modos nativos na primeira execução; escolhas
  já persistidas no launcher ou em Display Options não são substituídas
  pela autodetecção em um reinício. Testes sintéticos verificam
  seleção 1600x900@75, persistência JSON e respeito à escolha em
  monitor diferente; renderer OpenGL é default Windows, não override.
- Launcher reúne modos de janela, monitor, resoluções e refresh,
  upscaler/factor/preset, renderer, shaders GLSL externos, VSync,
  render scale, som e General, seleção de idiomas English / Spanish
  JSON / custom. Configuração física em `settings.json`, General/Sound
  retail em `Config.DAT`, aspecto narrativo em `Kotonoha.ini`, idioma
  em `language.txt`, caminhos externos de GLSL em `shader-paths.txt`.
- As opções são sincronizadas para impedir que o mixer em JSON e o
  `Config.DAT` se sobreponham por engano. Escritas em múltiplos arquivos
  não constituem transação atômica: uma falha tardia de gravação ainda
  pode exigir reconciliação manual, embora o launcher faça validações.
- **Male Voices nos sete epílogos LxH ainda não é seletivo:** cada ORS
  inicia um vídeo WMV sem áudio e um único OGG estéreo com diálogo,
  música e efeitos mixados em `PlaySe`, sem `PlayVoice` ou faixas por
  personagem. Mudar `MenVoice` no fluxo de `PlaySe` silenciaria também
  vozes femininas, música e SFX; por isso nenhum mute artificial foi
  aplicado. Uma separação fiel exige stems de áudio novos/reconstruídos
  e validação específica antes de modificar o pacote.

## Manutenção da variante compacta

A configuração estática foi criada com o manifesto
`games/school-days/tools/two-dll-vcpkg/vcpkg.json`, `Visual Studio 18 2026`,
`VCPKG_MANIFEST_DIR` apontando para seu diretório, `CMAKE_BUILD_TYPE=Release`
**mesmo com gerador multi-config** para ordenar caminhos vcpkg Release
antes de Debug, `VCPKG_TARGET_TRIPLET=x64-windows-static-md`,
`VCPKG_MANIFEST_MODE=ON`, `BUILD_SHARED_LIBS=ON`, `SDL_SHARED=ON`,
`SDL_STATIC=OFF` e opções `KOTONOHA_STATIC_ENGINE/IMGUI=ON`.
O caminho `PKG_CONFIG_EXECUTABLE` precisa apontar para
`<build>/vcpkg_installed/x64-windows/tools/pkgconf/pkgconf.exe`.
Desativar os outros jogos em CMake para esta distribuição e usar
`KOTONOHA_DEV_CONTROL=OFF`. **Não recompilar com o preset antigo
`school-days-dev-control4`**, pois ele usa o triplet dinâmico original e
reintroduziria as 29 DLLs. Compilar em build isolado e usar
`games/school-days/tools/publish_two_dll_l4.ps1` após validação,
com `-DryRun` primeiro e novo backup externo.

## Avisos para futura distribuição pública

Licenças em `Release/third-party-licenses`. FFmpeg e FriBidi vinculados
estaticamente têm requisitos LGPL de fontes, atribuição e possibilidade
de relinkagem/substituição; **avisos sozinhos podem ser insuficientes**.
Uma distribuição comunitária precisa avaliar e cumprir essas obrigações
e os direitos autorais dos 29 packs comerciais. Não subir GPKs comerciais
para o Git nem distribuir sem autorização. Esta entrega é a build local
de trabalho. Não houve commit nem push.
