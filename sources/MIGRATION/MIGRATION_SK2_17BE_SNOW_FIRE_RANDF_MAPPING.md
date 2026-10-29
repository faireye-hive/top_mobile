# SK2.17-BE — mapeamento DX9 `Randf` para SNOW/FIRE

## Lacuna medida

O FX coverage audit atual registra `feature:snow_fire_step` em **588 sistemas, 266 arquivos e 123 skills estáticos**. O inventário por tipo contém 566 SNOW e 504 FIRE; a contagem da feature é sua interseção específica, não a soma dos tipos. O vínculo de dados não prova execução em runtime.

## Original DX9 × porte OpenGL × mudança

| Original DX9 | Porte OpenGL antes | Mudança BE |
|---|---|---|
| `Randf(float)` usa `rand()%inteiro` e uma parte fracionária em passos de 0,001; para máximo fracionário menor que 1, usa `rand()%int(max*1000)`. `Randf(a,b)` soma `a` ao resultado de `Randf(b-a)` (`arquivo/fontes/Engine/include/MPParticleSys.h:6-15,26-44`). | O pool/movimento histórico usava um float contínuo `rand01` multiplicado pelo intervalo (`sources/Engine/include/top/HistoricalParticlePool.h:26-29`; `HistoricalParticleMotion.h`, antes desta etapa). Isso não reproduzia a quantização nem a distribuição produzida pelo helper DX9. | `historical_particle_random::dx9Randf` traduz a aritmética e o consumo de draws do helper por meio de um provedor determinístico (`sources/Engine/include/top/HistoricalParticleRandom.h`). Vida SNOW/FIRE, range de spawn, velocidade SNOW e sinais aleatórios passaram a usar esse mapeamento nos caminhos já opt-in (`HistoricalParticlePool.h`; `HistoricalParticleMotion.h`; `HistoricalParticleJitter.h`). |
| SNOW calcula velocidade e vida com `Randf`, faz duas escolhas `Rand(2)` para sinais horizontais, amostra a posição e sorteia o sinal de aceleração por frame (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3681-3720,3783-3789`). FIRE também sorteia vida/posição e usa `rand()%2` para aceleração (`MPParticleSys.cpp:3462-3522,3610-3618`). | A simulação guardava a sequência por slot/geração e usava sinais por limiar de float; a sequência global do Client não é uma entrada do renderer GL. | O porte preserva o comportamento discreto/modular do helper e mantém uma sequência determinística por slot/canal. A conversão de coordenadas DX9 XYZ/Z-up para OpenGL X/Z/Y-up permanece na fronteira existente; esta etapa não altera matrizes, blend, textura, culling ou escala. |

## Fatos, inferência e desconhecido

- **FATO:** o helper original pode produzir um intervalo mais estreito do que `[a,b]` sugere. Por exemplo, com vida 2 e divisor 4, `Randf(0.5,2)` calcula `0.5 + Randf(1.5)`; `Rand(1)` vale zero e a fração usa módulo 500, resultando em `[0.5,0.999]`. O teste fixture cobre esse caso derivado do código.
- **FATO:** `_FrameMoveFire` e `_FrameMoveSnow` sorteiam `rand()%2` por partícula viva em cada atualização (`MPParticleSys.cpp:3611-3614,3784-3787`).
- **FATO:** o Client tem chamadas `srand` em diferentes fluxos de UI/inicialização (`arquivo/fontes/Client/src/GameLoading.cpp:67`, `CreateChaScene.cpp:853`, `UIComposeForm.cpp:255`, `UIFoundForm.cpp:257`, entre outros), enquanto o renderer GL recebe um seed por efeito.
- **INFERÊNCIA:** a quantização e as operações modulo traduzem melhor as distribuições autoradas do que uma amostra contínua uniforme.
- **DESCONHECIDO:** sequência exata do `rand()` global do processo DX9 em uma sessão; ordem de chamadas fora dos dois controladores; comparabilidade visual. O seed por slot é determinístico, não é alegado como o estado global MSVC.

## Escopo e ativação

O mapeamento tem um switch próprio `TOP_HISTORICAL_PARTICLE_RANDF=1`, desligado por padrão. Sem ele, permanece a amostragem anterior; isso preserva as duas integrações existentes que verificam a aproximação contínua. Para a simulação SNOW/FIRE completa:

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_FRAME_ACCELERATION=1 \
TOP_HISTORICAL_PARTICLE_RANDF=1 \
./build/top_gl --profile
```

Desligue o novo comportamento removendo `TOP_HISTORICAL_PARTICLE_RANDF` ou definindo-o como `0` e reinicie; os outros switches controlam pool, movimento e aceleração. Para reverter este patch depois de aplicá-lo: `git apply -R SK217BE.patch`.

## Testes

`SK217AParticleRandomTests.cpp` foi estendido com uma sequência aleatória fixture injetável para os ramos inteiro/fracionário do `Randf`, quantização a 0,001, `rand()%2` e o intervalo histórico de vida `[0.5,0.999]`. Os testes existentes de divisor e movimento continuam presentes. No primeiro CTest completo, duas integrações que fixam a aproximação contínua falharam; não foram alteradas. O novo switch isola a tradução, deixando seus caminhos anteriores intactos por padrão.

Resultado final observado: build completo em configuração Debug, incluindo `top_gl`, terminou com exit code 0; CTest passou **138/138**; os dois testes de regressão passaram com o switch ausente; fixture focal ASan/UBSan passou (`detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`). A tentativa inicial antes do gate teve 2 falhas/138; após o gate, nenhuma falha.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Esta etapa não altera formatos `.par`/`.eff`, saves ou API pública. Não há declaração de paridade visual sem comparação do usuário com o Client DX9.
