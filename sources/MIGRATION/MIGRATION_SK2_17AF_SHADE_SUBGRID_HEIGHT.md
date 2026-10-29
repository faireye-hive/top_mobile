# SK2.17-AF — amostragem de altura e bloqueio de SHADE

## Lacuna medida

O FX audit Client encontrou 8 sistemas SHADE (`type=13`) em 8 arquivos PAR, ligados a 16 IDs de skill pelo grafo. A SK2.17-M projetava a grade, mas usava `MapFile::heightAt`, que interpola `cHeight`; o DX9 também consulta as alturas e o bit de bloqueio dos quadrantes `btBlock`.

## Evidência: DX9 original × porte GL × mudança

| Regra | Original DirectX 9 | Porte GL antes de AF | Mudança |
|---|---|---|---|
| Altura por vértice | `CMPShadeMap::MoveTo` converte a coordenada da grade para índices pequenos, desloca `iGridx/iGridy` em uma amostra nas bordas finais, obtém `GetGridHeight` e `GetTileHeight`, e usa a altura de quadrante somente quando excede a altura do tile e não está bloqueado (`arquivo/fontes/Engine/src/MPShadeMap.cpp:389-412`). | O builder SHADE chamava `MapFile::heightAt`, interpolando as quatro alturas `cHeight` dos tiles (`sources/Engine/include/top/HistoricalShadeMesh.h:46-58`; `sources/Engine/src/top/MapFile.cpp:173-190`). | `MapFile::shadeGroundHeightAt` seleciona o quadrante e aplica `max(tileHeight, subgridHeight)` somente se não bloqueado (`sources/Engine/src/top/MapFile.cpp:207-242`). `HistoricalShadeMesh` passa flags das bordas finais (`sources/Engine/include/top/HistoricalShadeMesh.h:51-58`). |
| Dados do MAP | `_UpdateHeightBuffer` preenche quatro alturas e quatro flags de bloco por tile (`arquivo/fontes/Engine/src/MPMap.cpp:1131-1142`). `getObjHeight` usa bits 0–5 em níveis de 5 cm e bit 6 como sinal; `IsBlock` lê bit 7 (`arquivo/fontes/Engine/include/MPTile.h:59-69,152-172`). | O parser já mantinha `btBlock[0..3]` no bloco de 15 bytes, mas somente RANGE2 consultava um quadrante (`sources/Engine/src/top/MapFile.cpp:192-205`). | O novo sampler decodifica sinal, altura e bloqueio sem alterar a API ou o layout serializado do MAP. |
| Sea mode e altura do tile | `CMPShadeMap` inicializa `_UpSea=false` (`arquivo/fontes/Engine/src/MPShadeMap.cpp:20-45,128-129`); quando falso, a comparação usa `GetTileHeight` diretamente (`:397-405`). | O caminho anterior usava apenas a altura interpolada. | O sampler aplica a regra padrão `_UpSea=false`; não inventa clamp de água. |
| Coordenadas/renderização | SHADE constrói a grade no XY do mapa e grava a altura no Z DX9 (`MPShadeMap.cpp:353-412`). | O GL usa X/Z horizontais e Y vertical na malha (`sources/Engine/include/top/HistoricalShadeMesh.h:51-58`; `EffectSystem.cpp:2190-2192`). | A consulta permanece em índices horizontais do MAP e seu retorno continua sendo a altura Y no GL. Nenhuma regra D3D de blend, winding ou matriz foi copiada; o blend e culling continuam no renderer GL existente. |

## FATOS, INFERÊNCIAS E DESCONHECIDOS

**FATOS:** O construtor de `CMPShadeMap` inicia `_UpSea=false`; `MoveTo` compara a altura do tile com a amostra do quadrante e não eleva uma célula bloqueada (`MPShadeMap.cpp:20-45,389-412`). O porte anterior usava `heightAt` interpolado; agora há consulta explicitamente opt-in com a correção da borda.

**INFERÊNCIA:** O índice de tile do mapa completo OpenGL corresponde ao mesmo tile mundial amostrado no buffer corrente DX9, para os vértices dentro da área carregada. A grade e os campos serializados são os mesmos; o limite do buffer DX9 pode produzir diferenças junto à borda da área visível.

**DESCONHECIDO:** O `MapFile` mantém acesso ao mapa inteiro, enquanto `MPMap::GetGridHeight/IsGridBlock` consultam apenas a janela de terreno carregada (`arquivo/fontes/Engine/include/MPMap.h:346-372`). Não há captura do raio da janela DX9 nem comparação visual em uma borda de janela. Curvas de animação, escala e textura SHADE também não foram comparadas em pixels.

## Alteração e audit

`TOP_HISTORICAL_SHADE_TERRAIN_GRID=1` ativa a amostragem exata do formato para o ramo já opt-in `TOP_HISTORICAL_SHADE=1`. Sem a nova variável, o ramo SHADE continua usando o sampler interpolado anterior. A tabela de cobertura registra `shade_terrain_grid_height` e o FX audit verifica o símbolo no renderer; o ranking mantém SHADE como parcial e inclui essa feature.

Audit Client executado após a mudança: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 23 lacunas, zero drift e zero falhas estruturais de decode. O JSON distingue 31 falhas estritas `.eff`, 30 com referência PAR/ID de cena e um `fboomerff.eff` sem referência direta, cujo uso segue desconhecido. SHADE: 8 sistemas/8 arquivos/16 skill IDs rastreáveis. Esses IDs não provam execução em jogo.

## Testes executados

- `.map` sintético cobre os quatro quadrantes, altura positiva/negativa, bit de bloqueio, comparação com `cHeight`, correção nas bordas, seção vazia e coordenada fora do mapa (`SeaTerrainParityTests.cpp`).
- Fixture SHADE confirma default desligado, callback das bordas e diferença no lote GL entre altura `cHeight` e a subgrade (`SK217MShadeMeshTests.cpp`, `SK217MShadeIntegrationTests.cpp`).
- Teste do FX audit confirma que o feature predicate aceita type 13, rejeita type 12 e aparece no ranking com evidência de código.
- Compilação Debug completa, incluindo `top_gl`, terminou com sucesso; CTest **129/129** passou.
- A fixture `.map` e a integração de SHADE passaram com AddressSanitizer e UndefinedBehaviorSanitizer (`ASAN_OPTIONS=detect_leaks=0`).
- Os testes exercitam regras e geometria/lotes; não executam o cliente DX9 e não provam paridade visual.

## Checklist de comparação visual

No mesmo mapa/posição e com o mesmo efeito, compare os vértices SHADE sobre superfície inclinada, quadrante elevado e quadrante bloqueado. Primeiro rode com o sampler interpolado:

```bash
TOP_HISTORICAL_BLEND=1 TOP_HISTORICAL_MODEL_TEXTURE=1 TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 TOP_HISTORICAL_PARTICLE_POOL=1 TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 TOP_HISTORICAL_SHADE=1 \
TOP_HISTORICAL_SHADE_TERRAIN_GRID=0 ./build/top_gl --profile
```

Reinicie nas mesmas condições alternando apenas a última variável para `1`. Compare também a borda da janela de terreno carregada. A geometria por fixture passou; ainda falta sua comparação contra o DX9.

## Ativação e reversão

```bash
TOP_HISTORICAL_SHADE=1 \
TOP_HISTORICAL_SHADE_TERRAIN_GRID=1 \
./build/top_gl --profile
```

Para retornar ao sampler interpolado, defina `TOP_HISTORICAL_SHADE_TERRAIN_GRID=0` ou remova essa variável. Para o renderer anterior ao SHADE, remova também `TOP_HISTORICAL_SHADE`. Reversão desta etapa: `git apply -R SK217AF.patch`.
