# SK2.17-F — RANGE2: altura de subgrade do mapa

## Resumo

O porte de RANGE2 passa a poder consultar a altura histórica armazenada em `btBlock[0]`, em vez da altura global interpolada do tile. A opção é independente e desligada por padrão: `TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT=1`. Para usá-la no RANGE2, continuam necessários `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e `TOP_HISTORICAL_RANGE2_COLLISION=1`.

## Comparação direta: DX9 x OpenGL

| Original DX9 | Porte GL antes desta etapa | Mudança SK2.17-F |
|---|---|---|
| RANGE2 move a partícula e consulta `GetGridHeight((int)x * 2, (int)y * 2)`: `arquivo/fontes/Engine/src/MPParticleSys.cpp:2226-2231`. | O adaptador mantinha X/Y horizontais do controlador e consultava `MapFile::heightAt(x,y)`, que interpola `cHeight` (`raw[7]`) entre tiles: `sources/Engine/src/top/EffectSystem.cpp:1167-1177`, `sources/Engine/src/top/MapFile.cpp:173-190`. | O caminho opt-in seleciona `MapFile::range2GroundHeightAt` via `HistoricalRange2Ground::sample`: `sources/Engine/include/top/HistoricalRange2Ground.h:9-17`, `sources/Engine/src/top/EffectSystem.cpp:1172-1177`. |
| A janela do mapa mapeia cada tile para 2x2 amostras; a coordenada consultada é o canto 0: `arquivo/fontes/Engine/src/MPMap.cpp:1131-1137`; `GetGridHeight` retorna a amostra pelo offset e retorna zero fora do buffer: `arquivo/fontes/Engine/include/MPMap.h:346-356`. | `MapFile::at` já lia os 15 bytes do `SNewFileTile`, mas não expunha as alturas de `btBlock`: `sources/Engine/src/top/MapFile.cpp:114-171`. | O novo método lê `raw[11]`, que é `btBlock[0]` na estrutura de 15 bytes: `arquivo/fontes/Libraries/Util/include/MPMapDef.h:31-39`; o leitor está em `sources/Engine/src/top/MapFile.cpp:192-205`. |
| `getObjHeight` usa os bits baixos `0..5` em níveis de 5 cm e bit 6 como sinal; o bit 7 continua sendo bloqueio: `arquivo/fontes/Engine/include/MPTile.h:59-69,154-172`. | A altura consultada era `cHeight` em décimos de metro, triangularmente interpolada. | O decodificador preserva a mesma sequência aritmética do DX9 (`nivel * 5 / 100`) e ignora o bit 7 para altura. Seção vazia usa zero, compatível com os quatro `btBlock[]` zerados pelo `MPTile::Init`: `arquivo/fontes/Engine/include/MPTile.h:100-119`; leitura em `MapFile.cpp:192-205`. |

## Tradução de coordenadas

**FATO:** o DX9 converte cada coordenada horizontal com cast para `int` antes de multiplicar por dois; o `GetGridHeight` opera na grade pequena. Como a consulta usa sempre coordenadas pares, RANGE2 lê o canto 0 de cada tile. No porte, as duas coordenadas horizontais do controlador viram `(x,z)` do mapa GL; o resultado permanece altura vertical e não importa estado, matrizes ou tipos D3D. `MapFile` usa índices inteiros de tile e o método recebe exatamente os inteiros fornecidos pelo simulador.

O switch afeta apenas o callback de chão usado por RANGE2. Outros usos de `MapFile::heightAt`, a navegação, os efeitos Frozen Arrow, Illusion Slash, Taunt e Berserk não são alterados por esta etapa.

## Teste e execução

A fixture sintética do `.map` verifica altura positiva, altura negativa, bloqueio no bit 7 sem contaminar a altura, seção vazia, limite externo e o switch ligado/desligado. Ela foi acrescentada a `sources/tests/SeaTerrainParityTests.cpp`; não muda as expectativas existentes.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Para comparação visual, mantenha as opções já usadas e inclua a nova:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_RANGE2_COLLISION=1 \
TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT=1 \
./build/top_gl --profile
```

Desligue somente esta consulta com `TOP_HISTORICAL_RANGE2_SUBGRID_HEIGHT=0`; remova a variável para usar o padrão desligado. Para reverter o patch depois de aplicá-lo: `git apply -R SK217F.patch`.

## Fato, inferência e desconhecido

**FATO:** o DX9 carrega quatro alturas de subgrade a partir de `btBlock[0..3]`; RANGE2 consulta explicitamente o canto 0 usando coordenadas pares. O formato serializado coloca `btBlock[0]` no offset 11 do tile de 15 bytes. **FATO:** o porte anterior usava `cHeight` interpolado, e o novo caminho alternativo lê o byte historicamente correspondente.

**INFERÊNCIA:** índices do `.map` no porte e a origem do mapa carregado pelo DX9 representam o mesmo tile mundial para a instância RANGE2. Isso é consistente com a API de mapa usada pelo porte, mas ainda precisa de confirmação visual em uma área com alturas de objeto diferentes da altura `cHeight`.

**DESCONHECIDO:** qual amostra a janela de terreno DX9 retorna fora da região atualmente carregada, pois seu buffer é limitado ao terreno visível. Para seções corrompidas/ilegíveis, o porte retorna zero de modo defensivo; equivalência histórica nesse erro não foi estabelecida. O relógio de 60 Hz e o RNG do simulador continuam aproximações das chamadas DX9. Não houve comparação visual com o cliente DirectX 9; RANGE2 permanece aproximado, não validado visualmente.

## Verificações executadas

- Configuração Debug com `TOP_BUILD_UI_TESTS=ON`: sucesso.
- Build completo, incluindo `top_gl`: sucesso.
- CTest: **104/104 passaram**.
- Teste da fixture com `-fsanitize=address,undefined`, `ASAN_OPTIONS=detect_leaks=0`: sucesso. A tentativa inicial de execução do binário em `/tmp` foi bloqueada pelo mount (`Permission denied`, exit 126); a execução repetida em `build/` passou.
- Auditoria somente-leitura executada contra `Client/`: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.639 partes `.eff`, 19 gaps, 307 skills com gaps, 31 `.eff` não totalmente decodificados, drift 0 e 0 strips diretos sem correspondência. RANGE2 aparece em 35 sistemas/19 arquivos/7 skills ligados por tabela.
- O relatório `fx_gap_ranking.csv` ainda conserva a anotação antiga de RANGE2 (“terrain hit callback pending”), anterior às etapas D/E/F. Os sete vínculos são IDs 4 Phoenix Slash, 7 Shot Array, 112 Meteor Shower, 220 Lightning Curtain, 231 Fishing, 246 Jellyfish Electric Bolt e 307 Deathsoul Lightning Curtain. São vínculos estáticos de recursos, não prova de execução em jogo.
- Não executei o jogo nem fiz comparação visual com DX9.
