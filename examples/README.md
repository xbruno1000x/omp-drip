# Exemplo com CJ original

A versao em ingles esta em [`cj-default-en.pwn`](cj-default-en.pwn), com
[instrucoes em ingles](README.en.md). Use `-Language en` no script de
preparacao para gerar `build/cj-default/cj-default-en.amx`. Os comandos sao
`/clothes`, `/confirm`, `/cancel`, `/cj` e `/resync`; ambas as versoes usam
o mesmo catalogo e pacote cliente.

`cj-default.pwn` e um **gamemode independente para open.mp**, com spawn na
Didier Sachs (interior 14), em `204.16, -165.76, 1000.52`, e skin 0. Usa apenas
`open.mp`, `omp-drip` e o catalogo gerado; os menus usam `CreateMenu`,
`OnPlayerSelectedMenuRow` e `OnPlayerExitedMenu`.

Ao abrir `/roupas`, o CJ vai para `215.2920, -156.1625, 1000.5234`, com angulo
`90.1563`. O jogador fica parado e a camera e posicionada 3.2 metros a frente,
olhando para ele. Ao sair do provador, a posicao/interior anterior, o controle
e a camera normal sao restaurados. A selecao de classe tambem usa camera frontal.

O visual inicial vem de `cj-default.overrides.json`: regata branca (`vest`),
cabelo original (`player_face`), jeans azul (`jeansdenim`) e tenis preto
(`sneakerbincblk`). Os outros 14 slots comecam vazios. Musculo e gordura sao
zero. O exemplo mantem a roupa confirmada ao morrer, mas reinicia ao reconectar.

## Gerar e compilar

Na raiz do repositorio, usando os arquivos locais informados:

```powershell
./scripts/prepare-cj-example.ps1 -GtaDirectory "C:/Program Files (x86)/GTA RIP"
```

O script apenas le os arquivos do jogo e grava os resultados em
`build/cj-default/`. Nao use `--base-player-img` neste exemplo: essa opcao
desabilita modelos originais para o caso de um CJ substituido por outro modelo.

Para tambem compilar, informe seu compilador Pawn e as pastas dos includes
open.mp/Pawn. Por exemplo, nesta maquina:

```powershell
./scripts/prepare-cj-example.ps1 `
  -GtaDirectory "C:/Program Files (x86)/GTA RIP" `
  -PawnCompiler "$env:APPDATA/sampctl/pawn/openmultiplayer/compiler/v3.10.11/pawncc.exe" `
  -PawnInclude @("../GTA Torcidas/dependencies/omp-stdlib")
```

Se os includes padrao do Pawn estiverem em outra pasta, acrescente-a a
`-PawnInclude`. Ao compilar por outra ferramenta, coloque
`build/cj-default/generated` antes de `include` nos caminhos de busca.
O `include/omp-drip-catalog.inc` do repositorio e um placeholder; o exemplo
recusa compilar com ele.

Resultados:

- `build/cj-default/cj-default.amx`: gamemode compilado, quando o compilador e informado.
- `build/cj-default/generated/omp-drip-catalog.inc`: IDs e visual inicial para Pawn.
- `build/cj-default/omp-drip/catalog.bin`: catalogo correspondente para o ASI.
- `build/cj-default/omp-drip/catalog.seed.json`: hashes dos arquivos usados.

## Instalar para testar

1. Compile os binarios nativos seguindo o README da raiz.
2. Gere o pacote cliente com o catalogo deste exemplo:

   ```powershell
   ./scripts/package-client.ps1 `
     -Asi "build/native/Release/omp-drip.asi" `
     -PlayerImg "C:/Program Files (x86)/GTA RIP/models/player.img" `
     -ClothesDat "C:/Program Files (x86)/GTA RIP/data/clothes.dat" `
     -ShoppingDat "C:/Program Files (x86)/GTA RIP/data/shopping.dat" `
     -Catalog "build/cj-default/omp-drip/catalog.bin" `
     -Output "dist/cj-default-client" `
     -Version "cj-default-demo"
   ```

3. Recompile o componente servidor para incorporar o manifesto desse pacote.
   Copie `omp-drip.dll` para `components/` do servidor open.mp.
4. Copie `build/cj-default/cj-default.amx` para `gamemodes/` e configure
   `pawn.main_scripts` como `["cj-default 1"]` no `config.json` do servidor.
   Execute este exemplo como gamemode, pois os callbacks do componente sao
   enviados ao script principal.
5. Instale o conteudo de `dist/cj-default-client` na pasta do GTA, preservando
   as subpastas. O cliente atual requer GTA SA 1.0 US, SA-MP 0.3.7-R3,
   ASI Loader e Mod Loader, conforme o README da raiz.

O AMX e o pacote cliente precisam usar o mesmo catalogo. O AMX sozinho nao
altera as roupas sem o componente servidor e o ASI instalado no cliente.

## Comandos

| Comando | Acao |
| --- | --- |
| `/roupas` | Entra no provador e abre o menu nativo de categorias, com 8 pecas por pagina e camera frontal. Escolher uma peca abre uma previa privada com menu de confirmacao. |
| `/confirmar` | Confirma a previa e sincroniza o visual com os jogadores em streaming. |
| `/cancelar` | Descarta a previa, recupera o ultimo visual confirmado e sai do provador. |
| `/cj` | Sai do provador, restaura a roupa inicial, remove acessorios e mantem a skin 0. |
| `/resync` | Sai do provador, cancela a previa e solicita novamente as aparencias em streaming. |

Cada categoria oferece "Restaurar padrao / remover": nos quatro
slots estruturais volta a peca inicial; nos opcionais usa ID zero. A escolha
tambem precisa de confirmacao. O menu da previa oferece "Confirmar roupa",
"Experimentar outra" e "Cancelar e sair", sem exigir comandos no chat.
Confirmar volta a lista de pecas; "Voltar as categorias" permite trocar o
tipo de roupa. Sair pelo controle nativo do menu descarta a previa e encerra
o provador. Abrir `/roupas` novamente tambem descarta a previa anterior.

Os itens sao identificados pela textura, respeitando o limite de 31 caracteres
do menu nativo. Cada pagina tem no maximo 12 linhas, incluindo navegacao.
Os menus sao criados uma vez e compartilhados; categoria, pagina e previa
permanecem individuais para cada jogador.

Os arquivos fornecidos geraram 291 itens de roupas e cabelos. Este exemplo
mostra torso, cabelo, pernas, calcados, colares, relogios, oculos, chapeus e
trajes especiais. O gerador atual nao importa o formato de tatuagens do
`shopping.dat` original; por isso elas nao aparecem no menu. Precos nao sao
cobrados e o exemplo nao implementa inventario ou banco de dados.

## Verificacao no jogo

- Entre e confirme o spawn na Didier Sachs com a roupa inicial do CJ.
- Abra `/roupas` e confira a posicao do provador, o angulo do CJ e a camera
  frontal. Confira que sair libera o movimento e restaura a posicao e a camera.
- Escolha uma peca em `/roupas`; outro jogador deve continuar vendo a roupa
  anterior ate `/confirmar`. Confira tambem navegacao entre paginas.
- Teste `/cancelar`, confirmar uma peca ja equipada e remover um acessorio.
- Morra durante uma previa: ao renascer, o visual deve ser o ultimo confirmado.
- Teste `/cj`, `/resync` e sair/voltar do alcance de streaming com dois clientes.

A compilacao verifica o Pawn; a renderizacao e a sincronizacao precisam
desses testes com o jogo e os binarios do plugin em execucao.
