# SK2.18-E — amostra histórica da grade no Part_trace

## Resumo

Corrigi a amostra de chão usada pela colisão do controlador `Part_trace`. A cadeia DX9 é `HitTestMap` → `CGameScene::GetGridHeight(float,float)` → `MPMap::GetGridHeight(int,int)`: a cena converte `int(world * 2)` e o mapa lê um dos quatro quadrantes de altura de objeto do tile. A implementação GL da etapa SK2.18-D consultava `MapFile::heightAt`, que interpola a superfície visual do tile; esses valores podem divergir. O caminho permanece condicionado a `TOP_HISTORICAL_PART_TRACE=1`.

**Correção do relatório/patch inicialmente entregue nesta etapa:** a primeira análise omitiu o wrapper `CGameScene` e afirmou incorretamente que bastava truncar a coordenada mundial e usar `range2GroundHeightAt`. Esse leitor só lê o quadrante zero de um tile inteiro. A fixture inicial usava valores uniformes e não distinguia essa falha. Este documento e o `SK218E.patch` foram corrigidos; `SK218E_CORRECAO.patch` é a alternativa incremental para quem aplicou a primeira versão E. Não aplicar os dois patches sobre a mesma base.

## Evidência: original DX9 × porte anterior × mudança

| Regra | Original DX9 | Porte antes desta etapa | Mudança |
|---|---|---|---|
| Consulta de colisão do projétil | `CMagicEff::HitTestMap(vPos)` compara `vPos->z` com a consulta da cena (`arquivo/fontes/Client/src/EffectObj.cpp:1226-1234`). `CGameScene::GetGridHeight(float,float)` chama o mapa com `int(fX*2.0f),int(fY*2.0f)` (`arquivo/fontes/Client/src/Scene.h:536-540`). | `HistoricalPartTrace` comparava a altura GL com `MapFile::heightAt(x,z)`, que interpola os triângulos da superfície (`sources/Engine/src/top/MapFile.cpp:173-190`). A primeira versão E usava incorretamente o leitor de quadrante zero. | A colisão usa `sceneGridHeightAt(x,z)`, que converte as coordenadas e seleciona o quadrante (`sources/Engine/include/top/HistoricalPartTrace.h:83-85`; `MapFile.cpp:207-229`). |
| Conteúdo da grade DX9 | `MPMap::GetGridHeight` acessa `_fHeightBuffer` em coordenadas da grade pequena (`arquivo/fontes/Engine/include/MPMap.h:346-357`); o mapa preenche essa grade com os quatro valores `MPTile::getObjHeight(0..3)` (`arquivo/fontes/Engine/src/MPMap.cpp:1131-1137`). `getObjHeight` decodifica `btBlock` com máscara de 6 bits, passos de 5 cm e bit de sinal (`arquivo/fontes/Engine/include/MPTile.h:59-69,170-173`). | O leitor RANGE2 existente lê somente `btBlock[0]` de um tile inteiro (`sources/Engine/src/top/MapFile.cpp:192-204`), portanto não implementa a consulta geral da cena em coordenadas fracionárias. | Adicionei `MapFile::sceneGridHeightAt(float,float)` para os quatro quadrantes e o Part_trace. A API existente e os consumidores RANGE2 permanecem com seu próprio contrato. |

### Tradução de coordenadas

O DX9 usa X/Y como plano e Z como altura; neste porte GL, esses eixos são X/Z e Y. `GetGridHeight(vPos->x,vPos->y)` da cena vira `sceneGridHeightAt(position.x,position.z)`. Dentro do leitor, `int(world*2)` seleciona tile `grid/2` e quadrante `(gridY%2)*2+gridX%2`. A comparação continua `position.y < ground`. O bit de bloqueio não altera a altura da amostra; o bit 64 define o sinal.

## Fixture sintética

A fixture põe a superfície visual em 0 m e os quatro quadrantes de um único tile em +1,00, -0,50, +1,50 (com bloqueio) e +3,15 m; os tiles vizinhos têm grade zero. Verifica a fronteira de meio tile, o sinal, o bit de bloqueio, a troca de tile e limites/não finitos. O projétil chega a (2,5; 2,5; 1,75) GL: o quadrante 3 exige parada, enquanto o quadrante zero e a malha visual não. O teste falhou com a primeira versão E e passou depois da correção; isso evita repetir o erro da fixture uniforme. Não substitui medição visual num mapa real.

## Auditoria atual e recursos `.eff`

Reexecutei `top_fx_coverage_audit` sobre o Client e `top_par_audit`:

- FX: 1.031 PAR, 1.227 EFF, 3.476 sistemas e 2.832 partes EFF; 27 gaps estáticos, 307 IDs de skills com gaps, drift 0, falhas estruturais 0 e strips não associados 0.
- PAR: 1.031 arquivos decodificados, 0 falhas, 3.476 sistemas e 110 referências ausentes.
- O CSV atualizado `build/fx-audit-sk218e/fx_eff_usefulness.csv` contém 31 recusas do parser estrito: todas recuperadas como `decoded_with_raw_model_params`; 30 com referências diretas por PAR e 1 sem referência direta (`fboomerff.eff`). Para as 31, `runtime_use_proven=no`. Portanto, a auditoria não prova uso runtime individual nem permite declarar `fboomerff.eff` inútil. Nenhum `.eff` foi descartado ou alterado.
- Frozen Arrow 93/Magic 20 é um recurso mode 2 listado na tabela Client e ligado por dependência estática. A chamada de colisão é do `Part_trace` geral; a dependência estática não prova que o jogador executou o efeito em runtime.

Comandos executados:

```bash
build/top_fx_coverage_audit --client Client --out build/fx-audit-sk218e \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp \
  --dx9-source arquivo/fontes/Engine/src/MPResManger.cpp \
  --dx9-particle-source arquivo/fontes/Engine/src/MPParticleSys.cpp \
  --dx9-particle-header arquivo/fontes/Engine/include/MPParticleSys.h \
  --dx9-character arquivo/fontes/Client/src/Character.cpp \
  --dx9-skill-attack arquivo/fontes/Client/src/STAttack.cpp
build/top_par_audit --client Client --out build/par-audit-sk218e
```

## Testes e execução

- `cmake --build build --target top_sk218d_part_trace_tests top_gl -j4`: exit 0; incluiu a fixture e o executável GL.
- Regressão negativa: com o leitor novo testado separadamente e a chamada antiga E ainda presente, `sk218d_part_trace_target_and_terrain` falhou com exit 8 em `Part_trace stops below CGameScene GetGridHeight`. Após trocar a chamada para `sceneGridHeightAt`, o teste passou.
- `ctest --test-dir build -R 'sk218d_part_trace_target_and_terrain|sea_terrain_parity' --output-on-failure`: 2/2 aprovados.
- `cmake --build build/sk218d-asan --target top_sk218d_part_trace_tests -j2`: exit 0; CTest focal com `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`: 1/1 aprovado. Logs corrigidos em `build/sk218e-correction-asan-build.log` e `build/sk218e-correction-asan-ctest.log`.
- Após a correção, `cmake --build build -j4` terminou com exit 0 e `ctest --test-dir build --output-on-failure` passou **142/142**. Logs: `build/sk218e-correction-full-build.log` e `build/sk218e-correction-ctest.log`.
- Não executei o Client em sessão nem comparei imagens DX9/OpenGL. A mudança é baseada na regra DX9 e fixture, mas não está visualmente validada.

## Limites

- O leitor GL carrega o mapa completo por seção; o DX9 `GetGridHeight` lê um buffer da janela ativa e retorna zero fora dela. O tratamento de posições fora da janela de renderização é uma diferença arquitetural ainda não coberta por esta fixture. Dentro da área carregada, a coordenada de grade e o decodificador `btBlock` foram comparados com o original.
- A correção muda somente a colisão do Part_trace quando `TOP_HISTORICAL_PART_TRACE=1`. O padrão desligado e a trajetória compatível permanecem inalterados.
- A sincronização de dano continua com o relógio de combate de distância inicial. O efeito visual/Result não altera `pendingAttack_`.
- Não promovo o switch a padrão sem comparação visual DX9/GL do usuário.

## Comparação visual solicitada

Com o mesmo mapa, posição, skill e câmera, teste Frozen Arrow com `TOP_HISTORICAL_PART_TRACE=0` e `=1`, mantendo `TOP_HISTORICAL_BLEND=1`, `TOP_HISTORICAL_MODEL_TEXTURE=1` e `TOP_FROZEN_RESULT_GROUND=1`. Observe um trecho de terreno em que altura visual e grade tenham diferença; compare o ponto em que o projétil para e onde o Result aparece. O `top_gl` usa o Client da raiz quando estiver presente.

```bash
TOP_HISTORICAL_PART_TRACE=1 \
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 ./build/top_gl --profile
```

Desligar: `TOP_HISTORICAL_PART_TRACE=0`. O código deste workspace já contém a correção: não reaplicar o patch aqui. Em uma cópia da base SK2.18-D, usar `git apply --check SK218E.patch` e `git apply SK218E.patch`. Para uma cópia com a primeira E incorreta, usar somente `git apply --check SK218E_CORRECAO.patch` e `git apply SK218E_CORRECAO.patch`. Para reverter integralmente E corrigida à base D: `git apply -R SK218E.patch`. Os dois patches têm caminhos começando por `sources/` e foram conferidos em suas respectivas bases pré-etapa.
