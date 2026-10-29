# SK2.17-AH — lacuna mensurada no relógio de colisão RANGE2

## Resultado

Corrigi a linha stale do catálogo que ainda dizia que o callback de impacto estava pendente desde SK2.17-B. O catálogo agora registra as etapas D/E/F e uma lacuna separada e verificável: o DX9 move e testa colisão uma vez por `FrameMove(dt)`, enquanto o porte reconstitui o histórico em passos nominais de 60 Hz para cada idade consultada.

Isto é uma atualização de auditoria e documentação, não uma correção do runtime. O próximo passo de comportamento exige armazenar a timeline por instância e perfil, porque uma simulação sem estado não conhece a sequência real de `dt` nem pode reproduzir exatamente o momento em que o DX9 testa o terreno.

## Evidência: DX9 original x OpenGL atual x mudança

| Original DX9 | Porte OpenGL atual | Mudança desta etapa |
|---|---|---|
| `_FrameMoveRange2` atualiza o tempo de vida do controlador com `dt`, move cada slot vivo por direção normalizada × velocidade × `dt`, e testa o terreno/limites logo após o movimento (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2195-2245`). | `HistoricalRange2Collision::simulate` deriva o número de atualizações de `floor(seconds*hz)` e tem `hz=60` por padrão (`sources/Engine/include/top/HistoricalRange2Collision.h:40-46,97-106`). `range2At` chama o simulador sem fornecer outro `hz` (`sources/Engine/src/top/EffectSystem.cpp:1331-1348`). | A tabela passa a classificar explicitamente essa diferença como `range2_frame_update_clock` e remove a afirmação obsoleta de que o callback de impacto está pendente (`sources/auditoria-historica/fx_port_coverage.csv:16,37`). O FX audit inclui os sistemas type 15 desta feature no ranking (`sources/Engine/include/top/FxCoverageAudit.h:171-185,425-440`). |
| No mapa, o DX9 usa `GetGridHeight((int)x*2,(int)y*2)`, marca o slot morto e envia `hitEffect`; sem mapa, aplica os limites sem clamp de terreno (`MPParticleSys.cpp:2226-2245`). | O GL já tem callback de colisão, disparo de `hitEffect`, reciclagem e opção de altura de subgrade, todos opt-in (`HistoricalRange2Collision.h:64-75,132-166`; `EffectSystem.cpp:1353-1405`; SK2.17-D/E/F). O porte mantém coordenadas históricas X/Y horizontais e Z vertical até a borda GL, onde reordena para X/Z horizontal e Y vertical (`EffectSystem.cpp:2050-2056`). | Não alterei colisão, ordenação de eixos, callback, ciclo de slots ou defaults. O novo item do ranking mede apenas a lacuna de cadência real de atualização. |

## Fato, inferência e desconhecido

- **FATO:** o DX9 aplica o `dt` do quadro em movimento e colisão; o helper GL usa uma reconstrução nominal de 60 Hz. O evento de colisão já está implementado no porte, portanto a nota antiga “terrain hit callback pending” era contraditória e foi corrigida na tabela-fonte.
- **FATO:** o audit atual mede 35 sistemas RANGE2, em 19 arquivos `.par` e 7 IDs de skill ligados pelo grafo. Esses vínculos não demonstram execução durante uma sessão.
- **INFERÊNCIA:** sob cadências que não sejam 60 Hz, ou com deltas variáveis, o quadro que detecta colisão e a posição enviada ao efeito de impacto podem divergir do DX9.
- **DESCONHECIDO:** distribuição global de `rand()` e paridade visual em cada mapa/skill. Não houve captura sincronizada com o Cliente DirectX 9.

Para implementar a cadência original, falta um estado persistente por instância/perfil que receba cada `dt`, mantenha timer compartilhado e gerações por slot, amostre terreno após cada movimento e entregue os eventos ao renderer. A movimentação da origem/path por frame também precisa entrar nessa timeline; não deve ser substituída pela posição final de uma consulta analítica.

## Auditoria e teste

O teste do FX audit agora exige que a tabela declare a cadência nominal de 60 Hz e a comparação com `FrameMove(dt)`. O audit do Client permanece somente-leitura e deve mostrar `range2_frame_update_clock` com os 35 sistemas ligados estaticamente.

Comandos usados nesta etapa:

```bash
cmake --build build --target top_fx_coverage_audit top_fx_coverage_tests -j$(nproc)
ctest --test-dir build -R '^ui05sk214_fx_coverage_audit$' --output-on-failure
build/top_fx_coverage_audit --client Client --out build/sk217ah-range2-clock \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

OpenGL/runtime não foi alterado nesta etapa; `top_gl`, CTest completo, ASan/UBSan e comparação visual não foram executados.

## Arquivos e reversão

- Código do audit: `sources/Engine/include/top/FxCoverageAudit.h`
- Tabela de cobertura: `sources/auditoria-historica/fx_port_coverage.csv`
- Fixture: `sources/tests/FxCoverageAuditTests.cpp`
- Estado: `sources/ESTADO_DO_PORTE.md`
- Patch incremental: `SK217AH.patch`

Reversão após aplicar: `git apply -R SK217AH.patch`.
