# SK2.17-BI — distribuição Randf no nascimento de RANGE2

## Lacuna medida

O FX coverage audit corrente (artefato `build/sk217bg-fx-audit`) inclui RANGE2 `type:15` em 35 sistemas, 19 arquivos PAR e 7 IDs estáticos de skill. Esses vínculos medem presença em dados, não execução. O controlador de colisão RANGE2 já existe atrás de `TOP_HISTORICAL_RANGE2_COLLISION`; esta etapa corrige somente a distribuição das duas coordenadas aleatórias usadas ao nascer um slot.

## Original DX9 × OpenGL atual × mudança

| Original DX9 | Porte OpenGL antes de BI | Mudança BI |
|---|---|---|
| `_CreateRange2` soma `Randf(_fRange[0])` e `Randf(_fRange[1])` à posição X/Y, mantendo `_fRange[2]` como deslocamento Z fixo (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2142-2155`). `Randf(float)` usa `rand()%N` para a parte inteira e passos de `rand()%1000 / 1000` na fração (`arquivo/fontes/Engine/include/MPParticleSys.h:6-37`). | `SystemTimeline::advance` gerava X/Y por `range * random01(...)`, uma distribuição contínua por slot (`sources/Engine/include/top/HistoricalRange2Collision.h`, caminho de nascimento). O modo sem mapa `at(...)` usava a mesma amostragem contínua (`HistoricalRange2Collision.h`, caminho de amostra). | Sob `TOP_HISTORICAL_PARTICLE_RANDF=1`, ambos os caminhos chamam o helper `deterministicRandf`; a faixa fica na distribuição/malha de `Randf` do DX9. Sem o switch, as expressões contínuas anteriores permanecem inalteradas. |

### Fatos, inferências e desconhecidos

- **FATO:** o DX9 soma coordenadas X/Y aleatórias e deixa Z como `_fRange[2]` fixo. O trecho DX9 também normaliza `_vDir`; a etapa não muda direção, colisão, relógio ou emissão.
- **FATO:** o helper GL `randomRange` seleciona o mapeamento por `TOP_HISTORICAL_PARTICLE_RANDF`; a timeline e o sampler `at` usam esse helper (`sources/Engine/include/top/HistoricalRange2Collision.h`).
- **FATO:** na integração OpenGL, o vetor de espaço DX9 X/Y/Z só é reordenado para X/Z/Y na fronteira de renderização (`sources/Engine/src/top/EffectSystem.cpp:2414-2420`). A nova lógica não altera essa convenção.
- **INFERÊNCIA:** a discretização por 0,001 reproduz melhor a distribuição dos valores de alcance esperada pelo código DX9 do que um float contínuo.
- **DESCONHECIDO:** a sequência global de `rand()` em uma sessão real. O DX9 consome o CRT compartilhado; o porte mantém uma sequência determinística própria por slot/geração. A posição exata de cada partícula não pode ser declarada idêntica.
- **DESCONHECIDO:** paridade visual, porque não foi feita comparação simultânea com o cliente DX9.

## Controle e reversão

O comportamento é aditivo e desligado por padrão. Ative com `TOP_HISTORICAL_PARTICLE_RANDF=1`; remova a variável ou use `=0` para retornar ao caminho anterior. Para desfazer somente esta etapa após aplicar o patch: `git apply -R SK217BI.patch`.

Exemplo de execução incluindo os switches de partículas já existentes:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_EFF_RAW_MODEL_PARAMS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_EMITTER_TIMER=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_RANGE2_COLLISION=1 \
TOP_HISTORICAL_PARTICLE_RANDF=1 \
./build/top_gl --profile
```

Para a comparação A/B, reinicie e repita com `TOP_HISTORICAL_PARTICLE_RANDF=0`, mantendo mesma cena, arquivo PAR, posição e FPS; compare com o cliente DX9.

## Testes e limites

O novo teste sintético `SK217BIRange2RandfTests.cpp` verifica o caminho antigo quando o switch está ausente, determinismo do caminho ligado, limites da faixa e quantização em 0,001. `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` terminou com código 0; `cmake --build build -j$(nproc)` terminou com código 0, incluindo `top_gl`; `ctest --test-dir build --output-on-failure` passou 139/139 (código 0). O fixture focal também passou compilado com `-fsanitize=address,undefined` e executado com `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1` (código 0). O patch passou `git apply --check`, aplicação, `git apply -R --check`, reversão e comparação byte a byte dos arquivos base numa cópia isolada dos arquivos afetados. O fixture não comprova ordem global de RNG nem aparência idêntica.
