# SK2.17-J — SNOW/FIRE shared emission timer

## Lacuna medida

O audit do Client encontrou **588 sistemas** SNOW/FIRE com `step > 0`, em 266 arquivos e 123 skills únicas: 238 SNOW e 350 FIRE (`build/sk217j-fx-coverage/fx_feature_coverage.csv`). O auditor lê os `.par` do Client e verifica no renderer atual a chamada `snowFireSystemAt`; o resultado atual foi `par=1031`, `eff=1227`, `systems=3476`, `eff_parts=2832`, `gaps=21`, `drift=0`, `failures=0`.

## DX9 original x porte GL x mudança

| Original DX9 (fato) | Porte antes desta etapa (fato) | Mudança desta etapa |
|---|---|---|
| `_CreateFire` e `_CreateSnow` incrementam o mesmo `_fCurTime` a cada tentativa de slot morto e zeram o timer quando `>= _fStep` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3462-3468,3681-3687`). | O avaliador anterior escalonava cada slot por `birth = slot * step` e reconstruía sua reciclagem individual, sem reproduzir tentativas ordenadas do timer compartilhado (`sources/Engine/include/top/HistoricalParticlePool.h:57-85` na base SK2.17-I). | `snowFireSystemAt` percorre frames nominais de 60 Hz e slots em ordem. Cada tentativa morta soma um tick; no limiar, zera o timer, cria a partícula e continua a varredura (`sources/Engine/include/top/HistoricalParticlePool.h:90-148`). |
| `_FrameMoveFire` e `_FrameMoveSnow` atualizam slots vivos; só o ramo morto chama `_Create*` e apenas enquanto `_bStop` é falso (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3664-3670,3752-3758,3826-3832`). A partícula encerra ao atingir todos os quadros; `GetCurFrame` reseta o tempo residual ao avançar a chave (`arquivo/fontes/Engine/include/MPParticleSys.h:116-129`). | O pool GL avaliava nascimentos por slot, portanto o número de tentativas por frame não dependia do total de slots mortos. | O replay conserva a ordem de slots, não tenta criar novamente no mesmo frame em que um slot termina, e agenda o fim pelas chaves em ticks do relógio nominal (`HistoricalParticlePool.h:113-146`). O resultado é fornecido uma vez ao renderer por sistema (`sources/Engine/src/top/EffectSystem.cpp:1826-1842`). |
| `_CreateSnow` zera o timer antes de retornar falso quando a componente vertical é zero (`MPParticleSys.cpp:3697-3701`); a tentativa falha, mas consome/reset o timer. | O snapshot antigo não representava tentativas de nascimento rejeitadas. | A função recebe `snowCanSpawn`; rejeições ainda consomem o timer, sem marcar slot vivo (`HistoricalParticlePool.h:125-138`; GL calcula o predicado em `EffectSystem.cpp:1826-1831`). |

## Diferença observável no fixture

O teste sintético usa FIRE com 20 slots, `step=0.15` e 60 Hz. No primeiro frame, o DX9 chama o criador até 20 vezes: o timer compartilhado gera duas criações nesse frame. O avaliador anterior por `slot*step` tornava vivo apenas o slot 0 no mesmo instante. O teste também cobre `step=0`, criação em grupo na primeira atualização, rejeição SNOW, expiração de chaves, reciclagem no frame seguinte ao da morte e parada de emissões preservando partículas vivas.

Esse resultado é uma comparação do algoritmo com um fixture, não captura de Client nem validação visual. Assets exemplares auditados: `01010003.par` (Greater Recover, type SNOW, count 80, step 0.6) e `01000030.par` (Tornado Swirl, type FIRE, count 80, step 1.0), ligados em `sources/auditoria-historica/dependencies.csv:118,102`.

## Ativação e compatibilidade

O modo é aditivo e desligado por padrão. Ele requer `TOP_HISTORICAL_PARTICLE_POOL=1` e também `TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1`; sem a segunda variável, o comportamento SK2.16-A permanece ativo. O teste antigo SK2.16-A esperava um nascimento analítico no primeiro snapshot e falhou quando o replay foi inicialmente ligado ao switch antigo. Mantivemos o teste e isolamos a política nova no switch adicional. Após o isolamento, os dois testes de integração antigos passaram sem alteração.

## Limites

- A simulação usa 60 Hz fixos, enquanto DX9 passa o `*_pfDailTime` real; intervalos, taxa de quadros e pausas podem mudar a sequência.
- As vidas continuam determinísticas e sem reproduzir a sequência de `rand()` DX9. A curva de quadros é aproximada pelo número de ticks por chave.
- O cálculo reconstitui do início até a idade atual ao gerar cada snapshot. O replay novo só é executado com `TOP_HISTORICAL_PARTICLE_POOL=1` e `TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1`; o custo máximo precisa ser observado em cenas com muitos efeitos simultâneos.
- Snow que falha por movimento vertical zero é coberto em lógica; uma auditoria anterior do Client não encontrou casos com `direction_z=0`. Não foi inferida paridade de RNG.
- Não houve comparação visual com DX9; SNOW/FIRE seguem aproximados, sem validação visual.

## Como testar

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
c++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/SK217JParticleEmissionTests.cpp \
  -o build/sk217j_particle_emission_asan
ASAN_OPTIONS=detect_leaks=0 ./build/sk217j_particle_emission_asan
```

Para comparar a implementação nova no Client, execute-a com `TOP_HISTORICAL_PARTICLE_POOL=1 TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1 TOP_HISTORICAL_PARTICLE_COUNT=1 TOP_HISTORICAL_PARTICLE_MOTION=1`. Para desligar só o timer novo, use `TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=0`; para voltar ao caminho anterior inteiro, desligue `TOP_HISTORICAL_PARTICLE_POOL=0`. Reverter esta etapa com `git apply -R SK217J.patch`.
