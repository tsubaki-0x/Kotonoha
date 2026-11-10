# School Days HQ L4 — staging local limpo para Windows (07/11/2026)

O utilitário `games/school-days/tools/stage_clean_local_l4.ps1` monta **uma nova pasta local**, semelhante à instalação inicial de um usuário, a partir dos binários e recursos já presentes em `build/school-days-dev-control4/games/school-days/Release`. O script é **somente leitura por padrão**. Não executa o jogo, não compila, não edita a Release existente nem os 29 GPKs comerciais.

## Inventário confirmado inicialmente

| Conteúdo | Quantidade | Destino na pasta nova |
| --- | ---: | --- |
| `SchoolDays.exe`, `SDL3.dll`, `SDL3_ttf.dll` | 3 | raiz |
| Packs comerciais originais | 29 | `Packs/` |
| Epílogos integrados `LxH.GPK` | 1 | `Packs/` |
| Imagens, ícone, splash, shader Anime4K/ArtCNN, fonte NotoSansJP e arte HD do launcher | 50 | `assets/` |
| Arte e mapas da interface General/Sound/Display | 12 | `display-options/` |
| `school-days-hq.ktnroute` e `styles.ass` | 2 | `game-assets/` |
| JSON EN/ES/zh-CN e respectivos metadados de revisão | 8 | `translations/` |
| Avisos e licenças dos terceiros | 16 | `third-party-licenses/` |

Total auditado antes da geração de arquivos auxiliares: **121 arquivos, 12.244.215.071 bytes**. Os dois DLLs são suficientes **como DLLs locais do pacote**, mas o Windows e o Microsoft Visual C++ Runtime x64 ainda fornecem dependências do sistema. A fonte Microsoft YaHei não acompanha a distribuição e seu fallback CJK depende da máquina do usuário.

**Arte HD concluída:** `assets/SchoolDaysHQ-Launcher.png` usa exatamente o PNG fornecido pelo usuário `homebg_waifu2x_art_scan_noise2.png`, 950×600, SHA256 `3d2586474a2fc4e163eca33cd89e4e6ca5faa9d97fab98d37fb07c27f01d82fe`, sem transcodificar ou redimensionar. `-Create` exige esse arquivo na Release.

O `Packs/` **da Release de desenvolvimento é uma junction** para `D:\SCHOOL DAYS HQ\Overflow\SCHOOLDAYS HQ\Packs`. O script rejeita links nos arquivos e, quando autorizado a criar o staging, copia individualmente o conteúdo dos 30 `.GPK` como arquivos regulares. A pasta nova não possui junction para a instalação original.

O script **não copia** os nove saves `Save/*.DAT`, `GlobalFlag.DAT`, `Config.DAT`, `settings.json`, `Kotonoha.ini`, `language.txt` (que atualmente referencia um JSON por caminho absoluto), `shader-paths.txt`, `LxH.mode`, `LxH.overlay`, arquivos `.pdb`/`.lib`, logs, capturas de teste e o antigo LEIA-ME com mapeamentos de saves. A pasta nova contém apenas `dir.txt` e `route.txt` portáveis, apontando para `Packs` e para o roteiro incluído, além de um aviso local. O primeiro início deve gerar suas próprias configurações e saves.

## Pré-teste somente leitura

No PowerShell, a partir de `D:\Dev\Kotonoha-v3`:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\school-days\tools\stage_clean_local_l4.ps1 `
  -Destination 'D:\Dev\Kotonoha-v3\build\school-days-dev-control4\clean-local-stage-20261107'
```

Sem `-Create`, nenhum diretório é criado. O padrão calcula SHA-256 dos 121 arquivos da lista positiva, inclusive todos os 29 GPKs e o LxH, e valida os hashes conhecidos de `Script.GPK` e `LxH.GPK`. Para um teste **mais rápido, apenas estrutural**, acrescente `-QuickDryRun`: ele **não** calcula o hash dos 28 GPKs comerciais grandes (mas verifica `Script.GPK` e `LxH.GPK`), e nunca deve ser considerado o gate criptográfico final.

O pré-teste também exige a lista exata de 30 packs, as duas DLLs locais, os recursos do jogo, o roteiro, as traduções e o destino ainda inexistente. Um JSON adicional (inclusive PT-BR privado) faz o preflight falhar até que seu uso seja revisto explicitamente; não existe cópia indiscriminada de `translations`.

## Geração da pasta após aprovação do EXE

**Staging criado e testado nesta entrega.** O exemplo abaixo é um registro da execução (não repita com o mesmo destino, pois ele já existe):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\games\school-days\tools\stage_clean_local_l4.ps1 `
  -Destination 'D:\Dev\Kotonoha-v3\build\school-days-dev-control4\School Days HQ - Clean Local L4' `
  -Create -ApprovedExeSha256 '9DB3DDF3995B199E810E8D64CB12F720EDB1C388B10E1386E5FC2379C9C241EC'
```

Para gerar o SHA a ser conferido pelo responsável pela Release:

```powershell
(Get-FileHash '.\build\school-days-dev-control4\games\school-days\Release\SchoolDays.exe' -Algorithm SHA256).Hash
```

Com `-Create`, os 121 arquivos são copiados e **cada destino é novamente comparado por tamanho e SHA-256** ao inventário antes da cópia. O script acrescenta os três pequenos arquivos de inicialização, cria `manifest-sha256.json` com caminhos relativos, tamanhos, papéis e hashes, e grava `manifest-sha256.sha256` com o hash do próprio manifesto. Uma interrupção deixa `INCOMPLETE-DO-NOT-USE.txt` no destino; não reutilize a pasta incompleta. O script não faz sobrescrita nem remoção de diretórios preexistentes.

**Validação final:** staging produzido com 121 arquivos fonte e três auxiliares, manifesto SHA256 `3D7F0DF94B8938D6B781740EEF1400D77F5DDED5D8AA4AEE5F5FB6CC1E6C4FAD`. A execução do `SchoolDays.exe` dessa pasta a partir de **outro diretório de trabalho**, com `--documents` isolado e **sem passar `--launcher`**, abriu a janela `School Days HQ - Launcher` e exibiu a nova arte HD. Nenhum save/configuração foi criado na pasta limpa. O `dir.txt` aponta somente para `Packs` e o `route.txt` inclui um roteiro relativo. A homologação de gameplay e fullscreen real permanece separada.

## Direitos e destino

**Uso local apenas.** Essa pasta nova reúne 29 GPKs comerciais do usuário mais o pacote LxH; não deve ser enviada à comunidade, publicada no Git, colocada em ZIP público, nem distribuída sem as autorizações pertinentes. Os avisos de terceiros acompanham o staging, mas uma distribuição pública ainda precisa verificar obrigações de licenças e relinkagem. Não confundir este staging privado com uma distribuição pública sem dados comerciais.
