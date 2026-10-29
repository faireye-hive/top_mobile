# SK2.17-O — integração discreta da aceleração do BLAST

## Lacuna medida

O ranking atual do FX audit encontra 423 sistemas `type=3 (BLAST)` em 118 arquivos e 101 skills. A feature nova, filtrando sistemas BLAST com aceleração `.par` não nula, encontra **208 sistemas, 66 arquivos e 64 skills alcançadas**. Exemplo de recurso ligado a skills: `cannon02.par` tem controladores type 3 e aparece para Cannon Shot e Bombardment em `sources/auditoria-historica/dependencies.csv`.

## Original DX9 x porte GL x mudança

| Original DX9 | Porte GL antes desta etapa | Mudança desta etapa |
|---|---|---|
| `_CreateBlast` inicializa a velocidade antiga aleatória, copia `_vAccel`, sorteia posição e vida (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3844-3877`). | `historical_particle_motion::at` calculava velocidade e spawn, mas convertia aceleração para `0.5*a*t²` (`sources/Engine/include/top/HistoricalParticleMotion.h:66-80`). | Sob `TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION=1`, uma correção substitui o termo contínuo pelo deslocamento discreto do passo nominal de 60 Hz. O switch fica desligado por padrão e só entra no caminho histórico de type 3. |
| A cada atualização, o DX9 move com `m_vOldPos * dt` e depois soma `m_vAccel * dt` a `m_vOldPos` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3923-3926`). A primeira atualização, portanto, não inclui aceleração no deslocamento daquele passo. | A fórmula contínua não reproduzia essa ordem exatamente em 60 Hz: após `n` passos inteiros, o termo DX9 é `a*dt²*n*(n-1)/2`. | A nova função inclui os passos inteiros e interpola o resto do intervalo usando a velocidade acumulada. O fixture integrado mede a correção no centroide da geometria OpenGL. |
| As posições DX9 são X/Y horizontais e Z vertical (coordenadas do motor D3D); a lógica não troca os componentes durante essa integração. | O GL já converte o deslocamento em `x=DX9.x`, `z=DX9.y`, `y=DX9.z` no limite do renderer (`EffectSystem.cpp:1902-1904`). | Mantém-se a conversão de eixos existente; somente o termo de aceleração type 3 muda sob o opt-in. Nenhuma matriz D3D é copiada para GL. |

## Fato, inferência e desconhecido

- **Fato:** o DX9 move usando a velocidade antiga e só depois atualiza essa velocidade com aceleração (`MPParticleSys.cpp:3923-3926`).
- **Fato:** a fórmula discreta derivada para `n` passos iguais é `a*dt²*n*(n-1)/2`; o termo anterior era `0.5*a*t²` (`HistoricalParticleMotion.h:74-80`).
- **Inferência:** em uma simulação nominal de 60 Hz, a correção aproxima melhor a posição do BLAST que a fórmula contínua. A integração numérica é conferida pelo teste unitário e pelo centroide do fixture.
- **Desconhecido/limite:** o DX9 usa o `dt` real de cada frame e `rand()` global; o replay do porte usa relógio nominal de 60 Hz e amostras determinísticas. Não reproduz uma sequência aleatória do cliente nem variações arbitrárias de FPS. Não houve comparação visual DX9/GL; o BLAST permanece **aproximado, não validado visualmente**.

## Ativação

O modo novo depende também dos mecanismos já existentes de pool e movimento:

```bash
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION=1 \
./build/top_gl --profile
```

Desligue omitindo a nova variável ou usando `TOP_HISTORICAL_BLAST_DISCRETE_ACCELERATION=0`; reinicie o processo ao trocar o modo. Para comparar o caminho anterior, mantenha pool e motion em `1` e defina o switch novo como `0`. Reversão: `git apply -R SK217O.patch`.

## Auditoria e testes

O FX audit adicionou `feature:blast_discrete_acceleration`, calculada dos campos de aceleração decodificados do `.par` e cruzada com a tabela existente de dependências. A fixture do auditor confirma que essa feature entra na cobertura/ranking e encontra o token do caminho renderer fora do loader.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_fx_coverage_audit --client Client --out build/sk217o-fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

Na execução desta etapa: `top_gl` e auditor compilados em Debug; CTest completo **113/113**; testes novos de trajetória, integração e auditoria passaram; o FX audit encontrou 208 sistemas/66 arquivos/64 skills na feature, `drift=0`, `failures=0` e 21 lacunas no ranking. ASan/UBSan dos dois fixtures O também passou (`detect_leaks=0`). Nenhuma comparação visual foi executada.
