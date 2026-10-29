# SK2.17-U — deslocamento aleatório SNOW/FIRE pelo `dt` real

## Lacuna medida

O FX coverage audit do Client encontrou **7 sistemas SNOW/FIRE com aceleração não nula em 5 arquivos `.par`**. O grafo de dependências atual não liga esses registros diretamente a skills; essa contagem prova presença dos dados, não uso em runtime. A auditoria geral leu 1.031 `.par`, 1.227 `.eff` e 3.476 sistemas, com `drift=0` e `failures=0`.

## Original DX9 x porte OpenGL x mudança

| Original DX9 | Porte OpenGL antes | Mudança SK2.17-U |
|---|---|---|
| FIRE recalcula a velocidade base com `oldPos * speed * dt`, escolhe `+accel*dt` ou `-accel*dt` com `rand()%2` e soma o resultado à posição (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3610-3618`). | O pool já avança SNOW/FIRE com chamadas reais `EffectSystem::step(dt)`, mas a aceleração aleatória estava somente no avaliador nominal de 60 Hz, ativado pelo modo estendido (`sources/Engine/include/top/HistoricalParticleJitter.h:18-35`; `EffectSystem.cpp:1981-1989`). | Com `TOP_HISTORICAL_PARTICLE_FRAME_ACCELERATION=1`, `SystemTimeline` acumula um sinal determinístico por slot e geração, uma vez por atualização viva, multiplicado pelo `dt` real (`HistoricalParticlePool.h:70-103`; `EffectSystem.cpp:1093-1099`). O nascimento não recebe deslocamento nesse mesmo passo, e um slot que termina também não recebe o passo. |
| SNOW escolhe o sinal em cada atualização viva e aplica `±accel*dt` à velocidade/deslocamento antes de mover a posição (`MPParticleSys.cpp:3783-3789`). | O desenho histórico já converte o movimento local DX9 para a fronteira GL: X/Y horizontais e Z vertical viram X/Z horizontais e Y vertical (`EffectSystem.cpp:1952-1954,1967-1980`). | A mudança adiciona o offset acumulado no espaço DX9 antes dessa conversão. Não altera blend, escala, textura, Frozen Arrow ou outros tipos. |

## Fato, inferência e desconhecido

- **FATO:** o DX9 usa um sinal aleatório por atualização viva para FIRE e SNOW, e o termo somado é `±accel*dt`; o código não integra essa aceleração como força persistente nem como `0.5*a*t²`.
- **FATO:** `EffectSystem::step(dt)` passa o delta de quadro real à timeline. O novo caminho só é consumido quando também estão ativos `TOP_HISTORICAL_PARTICLE_POOL=1`, `TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1` e `TOP_HISTORICAL_PARTICLE_MOTION=1`.
- **INFERÊNCIA:** o sinal determinístico por índice, geração e passo mantém repetibilidade útil para depuração, mas aproxima o RNG global do processo DX9.
- **DESCONHECIDO:** a sequência exata de `rand()` do DX9, pois ela é compartilhada com outros sistemas; correspondência de execução dos sete sistemas com skills; comportamento visual comparado ao Client DX9. Não houve captura visual.

## `.eff` recusados e utilidade

Na auditoria atual, `build/sk217u-fx-coverage/fx_decode_failures.csv` contém só o cabeçalho: **zero `.eff` falharam na decodificação**, portanto não há recusa atual para descartar como recurso inútil. O fixture do audit T também confirma que um `.eff` corrompido quando explicitamente referenciado por um `.par` aparece como falha referenciada.

Como contexto histórico, o relatório SK2.17-G registrou 31 arquivos que o parser da época recusava: 30 estavam ligados a 32 registros `model_eff` em `.par` válidos; `fboomerff.eff` não tinha ligação direta encontrada, mas o loader DX9 indexa/pré-carrega `.eff`, então sua falta de referência direta não prova que seja dispensável. Isso não é uma lista de falhas atual: a auditoria recente decodificou os 1.227 `.eff` do Client sem falhas.

## Ativação e reversão

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_FRAME_ACCELERATION=1 \
./build/top_gl --profile
```

O novo switch é desligado por padrão. Reinicie `top_gl` para alterná-lo. Para desligar somente a mudança, defina `TOP_HISTORICAL_PARTICLE_FRAME_ACCELERATION=0`. Reverter o patch incremental: `git apply -R SK217U.patch`.

## Testes executados

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isources/Engine/include sources/tests/SK217QParticleTimerTests.cpp \
  -o build/sk217u_particle_accel_asan
ASAN_OPTIONS=detect_leaks=0 build/sk217u_particle_accel_asan
```

Resultado observado: build Debug completo, incluindo `top_gl`, terminou com código 0; após corrigir um escape no teste de variável de ambiente, os alvos modificados recompilaram sem warning. CTest passou **122/122**. O fixture da timeline passou com ASan/UBSan (`detect_leaks=0`; LSan não foi executado). O FX audit Client confirmou 7 sistemas/5 `.par`, `drift=0`, `failures=0`; o CSV de falhas contém apenas o cabeçalho. Nenhum teste comprova paridade visual.

## Limites

O algoritmo reproduz a forma do termo e o ritmo por atualização, não o estado global de `rand()`. Ele depende do relógio por `dt` real do pool e, portanto, requer os três switches anteriores. Não foi feita comparação visual com Direct3D 9; a mudança é experimental, não declarada fiel/validada.
