# Gerador privado de GPKs L×H por idioma

O script **build_lxh_locale_gpks.py** converte os sete ORS estendidos do
LxH.GPK integrado, usando os sidecars L×H com 273 entradas.
Ele preserva falante, cronologia, todos os comandos e seus ticks; substitui
apenas o conteúdo textual de PrintText a partir do evento PlayMovie L×H.
Os ordinais seguem a ordenação do parser nativo: todos os comandos por
startTick, mantendo a ordem original para eventos no mesmo tick.

O LxH.GPK contém também o prefixo HQ. Para EN, esse prefixo vem
**exclusivamente do Packs/Script.GPK original**, identificado por SHA-256
E82DC824449381B9A132AA092740C4169EB3D77A820D969046B74CA1D9DF4ADF.
O gerador rejeita Script - english.GPK de translations.
Para PT-BR, o prefixo precisa vir do Script - Brazilian.GPK com 1859 entradas
e as sete cenas compatíveis. A integração não deve exibir falas HQ inglesas
quando PT-BR está selecionado.

Exemplo de execução a partir da raiz do repositório:

    python games/school-days/tools/build_lxh_locale_gpks.py --lxh-gpk "build/school-days-dev-control4/games/school-days/Release/Packs/LxH.GPK" --sidecars "build/school-days-dev-control4/games/school-days/Release/translations" --base-gpk "en=build/school-days-dev-control4/games/school-days/Release/Packs/Script.GPK" --base-gpk "pt-BR=build/school-days-dev-control4/games/school-days/Release/translations/Script - Brazilian.GPK" --locales en,pt-BR

O conversor apenas lê as fontes (em memória), sem extrair ou alterar nenhum
pack retail. Todos os arquivos novos ficam sob
tools/qa-lab/state/lxh-locales/run-ID/, com nomes en-lxh.GPK,
pt-BR-lxh.GPK e, se houver HQ espanhol compatível, es-lxh.GPK.
Cada GPK tem somente sete ORS, sem PNG, WMV ou OGG. O manifesto privado
registra SHA-256, tamanho, total de falas e status private-not-installed.
O CLI não permite selecionar um diretório de saída fora de QA state nem
sobrescrever um pack existente. Nenhum arquivo da Release ou save é escrito.

ES: o es-lxh.json original contém sete créditos Juan Ochoa; só os campos
de sentinela correspondentes são normalizados para U+200B na saída GPK,
sem qualquer modificação no JSON. Um es-lxh.GPK legítimo exige um Script.GPK
base em espanhol com HQ nas sete cenas e formato ORS compatível, passando
--base-gpk es=CAMINHO ou --base-overlay es=PASTA. Esse GPK base não foi
encontrado na Release examinada; gerar apenas o sufixo espanhol com HQ
em inglês seria incorreto.

ZH-CN: o GPK chinês bruto usa GB18030, caminhos incompatíveis e outra ordem
de eventos; **nunca o montar diretamente**. O conversor dedicado
`build_chinese_compatible_gpk.py` usa o `Packs/Script.GPK` retail autenticado
como autoridade dos 1859 caminhos e de todos os comandos, ticks, mídias e
escolhas. Aplica os 30.968 textos/opções de `zh-CN.json` (sem fallback
inglês) e a fala da única cena extra fora da KTRF localizada por tick/
falante na fonte chinesa bruta. O arquivo derivado é gravado somente em
`tools/qa-lab/state/chinese-compatible/run-ID/Script - chinese-compatible.GPK`.
Com esse GPK como `--base-gpk zh-CN=...`, o gerador L×H pode produzir
`zh-CN-lxh.GPK` (7 ORS/273 falas) em `tools/qa-lab/state/lxh-locales/`.
Exemplo de reconstrução chinesa a partir dos arquivos pessoais instalados:

    python games/school-days/tools/build_chinese_compatible_gpk.py --retail-gpk "build/school-days-dev-control4/games/school-days/Release/Packs/Script.GPK" --chinese-gpk "build/school-days-dev-control4/games/school-days/Release/translations/scripts/Script - chinese.GPK" --zh-json "build/school-days-dev-control4/games/school-days/Release/translations/jsons/zh-CN.json" --ktrf "games/school-days/game-assets/school-days-hq.ktnroute" --build

O montador do Windows continua comparando todas as cenas traduzidas com o
Script.GPK retail; se os comandos/timings divergirem a inicialização falha
explicitamente. A tradução russa usa `Script - Russian.GPK` e `ru-lxh.GPK`;
uma legenda extra de Taisuke (cena 03-KB-D10, frames 403–577), sem alteração
de áudio/controle, é reconhecida exclusivamente para `ru` nessa cena.

Testes com ORS e GPKs sintéticos, sem modificar assets licenciados:

    python -m unittest discover -s games/school-days/tools -p test_build_lxh_locale_gpks.py -v

Esses testes não substituem testes funcionais do jogo. A auditoria read-only
contra o LxH.GPK privado também verificou as 273 chaves de evento exatas.
