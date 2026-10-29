# SK2.17-AI — timeline RANGE2 com o `dt` efetivo

## Resultado

Adicionei um modo opcional que consome cada `dt` recebido por `EffectSystem::step` e mantém o estado do controlador e dos slots RANGE2 por instância e perfil. O replay segue a ordem observada em `_FrameMoveRange2`: soma `dt` ao relógio do controlador, visita slots em ordem, move slots vivos, testa colisão, não recria um slot no mesmo update em que colide e avança o temporizador compartilhado uma vez por tentativa de criação de slot morto.

Ativação: requer `TOP_HISTORICAL_PARTICLE_EXTENDED=1`, `TOP_HISTORICAL_RANGE2_COLLISION=1` e `TOP_HISTORICAL_RANGE2_ACTUAL_DT=1`. Todas ficam desligadas por padrão. O replay nominal de 60 Hz continua como fallback quando a nova variável não está ativa.

## Evidência: DirectX 9 original x OpenGL atual x mudança

| Original DX9 | Porte OpenGL antes | Mudança SK2.17-AI |
|---|---|---|
| `_FrameMoveRange2` soma `dt` ao tempo do controlador e pode pará-lo antes de percorrer os slots (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2201-2204`). | O modo de colisão reconstituía o histórico com ticks fixos e simulava todos os slots a partir da idade consultada (`sources/Engine/include/top/HistoricalRange2Collision.h:97-106`). | `SystemTimeline::advance` guarda o tempo do controlador e os slots vivos/mortos entre chamadas; o novo caminho recebe o `dt` de cada etapa em vez de estimá-lo por FPS (`HistoricalRange2Collision.h:176-271`). |
| Para cada slot vivo, o DX9 move pela direção normalizada vezes velocidade e `dt`, consulta o terreno e, ao colidir, mata o slot e chama `SendResMessage` (`MPParticleSys.cpp:2218-2245`). | O renderer chamava `simulate(age,...)` sem passar cadência (`EffectSystem.cpp:1331-1348`), e os impactos eram reencontrados por geração consultando o histórico completo. | `EffectSystem::step` guarda o intervalo por instância (`EffectSystem.cpp:1040-1075`); `build` consome os intervalos na ordem, calcula origem e caminho em cada idade registrada, usa o sampler de terreno disponível e enfileira o `hitEffect` no update que detectou a colisão (`EffectSystem.cpp:1381-1435`). |
| Para slot morto enquanto o controlador não parou, `_CreateRange2` acrescenta `dt` ao temporizador compartilhado, zera-o quando alcança `_fStep` e cria o slot (`MPParticleSys.cpp:2142-2193,2256-2259`). | O replay anterior distribuía emissões em uma grade nominal de 60 Hz. | A timeline persiste `emitterTime`, gerações e estados; o hit deixa o slot morto para o próximo update, sem reciclagem no mesmo loop. Coordenadas permanecem X/Y horizontais e Z vertical durante a simulação; na borda OpenGL, o renderer reordena para X/Z horizontal e Y vertical. |

## Fatos, inferências e desconhecidos

- **FATO:** o código original atualiza RANGE2 uma vez por chamada de `FrameMove` e usa o `dt` fornecido; a nova timeline consome os deltas da chamada `EffectSystem::step` sem conversão para D3D.
- **FATO:** o switch é opt-in e depende dos dois switches de RANGE2 já existentes. `EffectSystem` armazena os deltas até `build`, que recebe o `MapFile` necessário para consultar o chão.
- **INFERÊNCIA:** em sequências de `dt` variáveis, o novo caminho deve alinhar o quadro de teste e a posição do impacto melhor que o replay fixo de 60 Hz, desde que `step` e `build` sejam chamados na ordem normal do jogo.
- **DESCONHECIDO:** o RNG global DX9 continua substituído por amostras determinísticas por slot/geração. Casts presos a dummies móveis não usam essa timeline, pois o porte não retém o histórico dos dummies para cada `dt`; esses casos mantêm o caminho anterior. A semântica de atraso na fronteira entre frames e qualquer diferença visual precisam de captura DX9/OpenGL.

## Testes

Foram adicionados casos unitários para deltas irregulares, movimento de colisão sem mapa, não reciclar no mesmo update e nova geração no update posterior. A fixture integrada roda o RANGE2 em etapas de 1/60 s e exige o `.par` de impacto autorado após a colisão. Resultado observado: build Debug completo incluindo `top_gl` concluído; CTest completo **129/129**; fixtures unitária e integrada também passaram com ASan/UBSan (`detect_leaks=0`). A auditoria Client registrou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes EFF, 25 lacunas, zero deriva e zero falhas estruturais no modo diagnóstico. Essas verificações não demonstram paridade visual.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build -R 'ui05sk217d_range2_(collision|hit_effect_integration)$' --output-on-failure
ctest --test-dir build --output-on-failure
```

## Checklist visual

Use o mesmo Client, mapa, efeito e cadência na comparação DX9/OpenGL:

```bash
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_RANGE2_COLLISION=1 \
TOP_HISTORICAL_RANGE2_ACTUAL_DT=1 \
./build/top_gl --profile
```

Compare o ponto da colisão, altura enviada ao impacto, ordem de reciclagem e comportamento com FPS variável. Para desligar somente a nova timeline, defina `TOP_HISTORICAL_RANGE2_ACTUAL_DT=0`; para reverter o patch após aplicá-lo, use `git apply -R SK217AI.patch`.

Não houve comparação visual nesta etapa; RANGE2 continua aproximado, não validado visualmente.
