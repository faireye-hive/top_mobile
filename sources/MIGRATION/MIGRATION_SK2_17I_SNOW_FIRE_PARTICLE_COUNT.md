# SK2.17-I — SNOW/FIRE authored particle count

## Gap medido

O DX9 cria `_iParNum = iNumPart` e percorre todos os slots em SNOW e FIRE (`arquivo/fontes/Engine/src/MPParticleSys.cpp:259-270,3579-3581,3752-3754`). Antes desta etapa, o renderer GL desenhava `min(profile.count, 24)` independentemente do valor gravado. O decoder preserva `profile.count` até 100 (`sources/Engine/src/top/EffectSystem.cpp:254`), portanto o corte ocorria depois da leitura, na preparação dos billboards.

Uma nova execução de `top_fx_coverage_audit --client Client --out build/sk217i-fx-coverage` encontrou 1.031 `.par`, 3.476 sistemas e zero falhas. A feature auditora adicionada agora verifica `type in {SNOW,FIRE} && count > 24` contra o Client e encontra:

| Dados medidos | Valor |
|---|---:|
| Sistemas SNOW com count > 24 | 85 |
| Sistemas FIRE com count > 24 | 54 |
| Sistemas no total | 139 |
| Arquivos `.par` | 74 |
| Skills vinculadas | 44 |
| Slots autorados acima do teto 24, somados por sistema | 3.647 |

Os valores de skills são únicos dentro da feature; sistemas e slots não representam ocorrências simultâneas em tela. A auditoria grava `feature:count_over_24`, conta dados PAR e confirma que o símbolo de seleção aparece fora do loader GL (`sources/Engine/include/top/FxCoverageAudit.h:147-165,355-366`; regra `sources/auditoria-historica/fx_port_coverage.csv`).

## Original DX9 x porte GL x mudança

| Original DX9 (fato) | Porte antes desta etapa (fato) | Mudança desta etapa |
|---|---|---|
| `CMPPartSys::Create` atribui `_iParNum = iNumPart` e aloca esse número de partículas (`arquivo/fontes/Engine/src/MPParticleSys.cpp:259-270`). | Na snapshot SK2.17-H, `EffectSystem` limitava `count` com `std::min(profile.count, 24)` (`sources/Engine/src/top/EffectSystem.cpp:1774`, antes desta alteração). | `HistoricalParticleCount::resolve` usa o valor autorado para type 1/2 somente quando `TOP_HISTORICAL_PARTICLE_COUNT=1`; o valor padrão continua limitado a 24 (`sources/Engine/include/top/HistoricalParticleCount.h:8-19`; integração em `sources/Engine/src/top/EffectSystem.cpp:1774-1776`). |
| `_FrameMoveFire` e `_FrameMoveSnow` iteram `n < _iParNum` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3579-3581,3752-3754`). | O porte limita type 1/2 junto com os outros tipos, embora os dados conservem até 100 (`sources/Engine/src/top/EffectSystem.cpp:254`). | A ampliação é restrita a SNOW/FIRE; singletons MODEL/STRIP, BLAST e todos os outros tipos mantêm a contagem anterior. Nenhuma escala, movimento ou textura foi alterada. |

## Teste do auditor

O `FxCoverageAuditTests.cpp` gera um `.par` sintético FIRE com count 30. O auditor deve encontrar exatamente uma instância `count_over_24`, apontar o uso do resolvedor no renderer e incluí-la no ranking; os checks anteriores de tipo, dependência, drift e segurança continuam ativos. Fixture de seleção adicional verifica o comportamento default (24), opt-in (count completo), escopo só SNOW/FIRE e singleton.

## Uso e reversão

```bash
TOP_HISTORICAL_PARTICLE_COUNT=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
./build/top_gl --profile
```

Desligar a mudança com `TOP_HISTORICAL_PARTICLE_COUNT=0`. Reverter esta etapa com `git apply -R SK217I.patch`.

## Testes executados

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/SK217IParticleCountTests.cpp \
  -o build/sk217i_particle_count_asan
ASAN_OPTIONS=detect_leaks=0 ./build/sk217i_particle_count_asan
```

Resultado observado: `top_gl` compilou; CTest passou **106/106**; o teste focal passou com ASan/UBSan e `detect_leaks=0`. O audit atualizado passou com `drift=0`, `failures=0`; identificou 139 sistemas/44 skills no gap. Não foi feita comparação visual com DX9. A contagem corrigida ainda requer avaliação visual e de desempenho com os assets reais.

Checklist visual direcionado: comparar Greater Recover (`01010003.par`, type SNOW, count 80; vínculo `sources/auditoria-historica/dependencies.csv:118`) e Tornado Swirl (`01000030.par`, type FIRE, count 80; vínculo `sources/auditoria-historica/dependencies.csv:102`) no DX9 e GL. Observar contagem aparente, densidade, sobreposição, curva/alpha e custo de renderização; repetir com `TOP_HISTORICAL_PARTICLE_COUNT=0` como controle.

## Limite conhecido

Esta etapa corrige quantidade disponível ao renderer, não o escalonamento de nascimento nem o fluxo exato de `rand()`/aceleração. O pool SK2.16 ainda aproxima o timer compartilhado por slot; essa continua sendo a próxima diferença comprovada para SNOW/FIRE.
