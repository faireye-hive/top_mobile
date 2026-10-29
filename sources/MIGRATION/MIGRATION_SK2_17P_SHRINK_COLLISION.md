# SK2.17-P — movimento e colisão do SHRINK

## Lacuna medida

O FX coverage audit atual lista 42 sistemas type 12 (SHRINK) em 32 `.par`, alcançando 53 skills. O type 12 permanece `partial`; a implementação OpenGL anterior usava fatores de posição/velocidade próprios e um limite esférico de `0.06`, sem correspondência no código-fonte DX9.

## Original DX9 x porte GL x mudança

| Original DX9 | Porte GL antes desta etapa | Mudança desta etapa |
|---|---|---|
| `_CreateShrink` escolhe o centro como `_vPos + range/2`, sorteia cada coordenada em `[0, range]` e normaliza `centro - posição` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:2439-2446`). | A posição era amostrada com ruído multiplicado por `0.22`/`0.18` (`sources/Engine/src/top/EffectSystem.cpp:2204-2206`). | `TOP_HISTORICAL_SHRINK=1` usa a faixa autorada para gerar a posição relativa ao centro. O sorteio do porte é determinístico por seed; a sequência global `rand()` do DX9 continua diferente. |
| `_FrameMoveShrink` soma direção normalizada vezes velocidade autorada e `dt`, depois chama `PointPointRange(..., 0.5)` (`MPParticleSys.cpp:2518-2521`). `PointPointRange` compara cada eixo com `< 0.5` (`arquivo/fontes/Engine/include/MPParticleSys.h:54-58`). | O GL calculava um deslocamento limitado com `abs(velocity) * age * 0.12`, usava um raio de `0.06` e não reproduzia o teste por eixo (`EffectSystem.cpp:2207-2215`). | O modo histórico move em passos nominais de 60 Hz, usa o sinal e magnitude da velocidade do `.par`, e encerra a geometria quando os três eixos entram estritamente no limite DX9. |
| O DX9 atualiza a curva por partícula usando `GetCurFrame` e os quadros seguintes (`MPParticleSys.cpp:2502-2516`). | A curva do type 12 usava a idade geral do efeito. | Sob o opt-in, a curva usa a idade escalonada individual do slot (`EffectSystem.cpp:1866-1878`). |
| O motor DX9 armazena coordenadas X/Y horizontais e Z vertical. | O renderer GL usa X/Z horizontais e Y vertical. | A saída traduz X→GL x, Y→GL z e Z→GL y na fronteira do renderer (`EffectSystem.cpp:2200-2202`); não copia matrizes D3D. |

## Fato, inferência e desconhecido

- **Fato:** a fórmula de deslocamento e o teste de chegada por caixa vêm de `_CreateShrink`, `_FrameMoveShrink` e `PointPointRange`, nas linhas citadas acima.
- **Inferência:** uma posição determinística em `[0, range]` e replay nominal de 60 Hz são adequados para teste reproduzível do comportamento, mas não geram a mesma sequência aleatória do cliente.
- **Desconhecido/limite:** o porte ainda agenda slots com `particleAge - slot * step`; o DX9 usa um acumulador compartilhado, `dt` real e recicla slots depois da morte ou fim de vida. Esses eventos continuam aproximados. A escala de billboard do renderer também não foi alterada nesta etapa.
- Não houve comparação visual. SHRINK permanece **aproximado, não validado visualmente**.

## Ativação e reversão

```bash
TOP_HISTORICAL_SHRINK=1 ./build/top_gl --profile
```

O modo depende apenas da nova variável e fica desligado por padrão. Compare com o caminho atual omitindo a variável ou usando `TOP_HISTORICAL_SHRINK=0`; reinicie `top_gl` após trocar o valor. Para reverter somente esta etapa: `git apply -R SK217P.patch`.

## Auditoria e testes

Fixture unitário verifica o sorteio determinístico, movimento por velocidade autorada e o limite estrito por eixo; fixture integrado serializa um `.par` type 12 e compara o centroide OpenGL com o resultado do helper. O Client foi auditado com `top_fx_coverage_audit`: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 21 lacunas e zero falhas de decodificação.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_fx_coverage_audit --client Client --out build/sk217p-fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

Execução registrada: build Debug completo incluindo `top_gl`; CTest **115/115**; os dois testes P passaram também sob ASan/UBSan (`detect_leaks=0`). Não houve comparação visual DX9/GL.
