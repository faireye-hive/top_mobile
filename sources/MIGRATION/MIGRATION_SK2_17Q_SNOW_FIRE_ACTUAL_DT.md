# SK2.17-Q — temporização do emissor SNOW/FIRE pelo `dt` real

## Lacuna medida

O FX coverage audit mede 588 sistemas SNOW/FIRE com `_fStep` não nulo em 266 `.par`, alcançando 123 skills. O ranking continua `source_inspection` porque a vida/movimento aleatórios não reproduzem o `rand()` global do DX9.

## Original DX9 x porte GL x mudança

| Original DX9 | Porte GL antes desta etapa | Mudança desta etapa |
|---|---|---|
| `_CreateSnow` e `_CreateFire` somam o `*_pfDailTime` ao mesmo `_fCurTime`, comparam `>= _fStep` e zeram o acumulador ao emitir (`MPParticleSys.cpp:3462-3468,3681-3687`). | `snowFireSystemAt` reexecutava o controlador em uma sequência nominal de 60 Hz (`HistoricalParticlePool.h`, função `snowFireSystemAt`). | `SystemTimeline` agora recebe as chamadas reais `EffectSystem::step(dt)` e mantém o timer compartilhado por instância (`EffectSystem.cpp:1037-1084`; `Platform/linux/main.cpp:2787`). |
| `_FrameMoveFire`/`_FrameMoveSnow` percorrem slots em ordem; atualizam slots vivos e só tentam criar nos mortos (`MPParticleSys.cpp:3579-3618,3752-3832`). | O replay por idade não preservava a sequência por frame nem a perda do excesso do timer quando ele zerava. | A timeline mantém idade/frame por slot, visita na ordem do índice, não recicla o slot no mesmo update em que morre e zera o timer sem carregar excesso (`HistoricalParticlePool.h:39-113`). |
| A vida é sorteada por slot; `GetCurFrame` recebe `dt` real e avança no máximo um quadro por atualização (`MPParticleSys.cpp:3497-3502,3584-3597,3714-3719,3757-3770`). | O porte atribuía nascimento e morte em ticks sintéticos. | O novo estado usa `dt` recebido, vida determinística por slot para reprodutibilidade e fase de curva baseada no quadro/temporizador do slot. |
| DX9 guarda X/Y horizontais e Z vertical. | O renderer OpenGL mapeia o espaço para X/Z horizontais e Y vertical. | Esta etapa altera somente emissão/vida; a conversão de coordenadas existente permanece na fronteira do renderer. |

## Fato, inferência e desconhecido

- **Fato:** DX9 usa um único `_fCurTime` do sistema e cada tentativa de criação de slot morto incrementa esse timer; ao atingir `_fStep`, ele volta a zero. Cada `FrameMove` visita os slots em ordem e não cria um novo no mesmo ramo que acabou de marcar o antigo morto.
- **Fato:** o executável chama `effects.step(dt)` com o delta variável do loop Linux (`sources/Platform/linux/main.cpp:2787`).
- **Inferência:** manter uma timeline mutável por sistema/instância representa melhor a sequência de emissão em taxas de quadro diferentes que reconstruir o estado a partir da idade total usando 60 Hz.
- **Desconhecido/limite:** o estado usa sorteio determinístico por slot, não o estado global de `rand()`; a direção/posição aleatória de nascimento e os sinais de aceleração por frame continuam diferentes. A detecção de `FireHit` ainda é processada no caminho atual de construção do frame, podendo determinar a parada no frame seguinte. Não houve comparação visual.

## Ativação e reversão

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1 \
./build/top_gl --profile
```

Ambas as variáveis precisam ser `1`; omitidas ou definidas como `0`, mantém-se o caminho anterior. Reinicie `top_gl` ao alternar. Para reverter somente a etapa: `git apply -R SK217Q.patch`.

## Auditoria e testes

O audit atualizado confirma 588 sistemas com `_fStep` e procura `snowFireTimelines` no renderer. O audit completo do Client encontrou 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 21 lacunas, zero deriva, zero falhas de decodificação e 307 skills alcançadas por ao menos uma lacuna.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_fx_coverage_audit --client Client --out build/sk217q-fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

Resultados observados: configuração e build Debug completos concluídos, incluindo `top_gl`; CTest completo passou **117/117**. Os dois testes Q também passaram com AddressSanitizer e UndefinedBehaviorSanitizer (`ASAN_OPTIONS=detect_leaks=0`; LSan não foi executado). O audit Client terminou com `drift=0`, `failures=0`; a lista `fx_decode_failures.csv` contém apenas cabeçalho, então não há falha atual de `.eff` para classificar como usada ou dispensável. Não foi feita comparação visual DX9/GL.
