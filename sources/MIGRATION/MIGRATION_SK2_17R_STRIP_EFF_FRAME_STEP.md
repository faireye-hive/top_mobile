# SK2.17-R — avanço por quadro dos filhos `.eff` de STRIP

## Lacuna medida

O FX coverage audit atualizado mede 1.041 sistemas type 6 em 504 `.par`, ligados a 242 skills. A auditoria `.par` executada no Client decodificou 1.031 arquivos e encontrou 875 sistemas type 6 cujo campo de modelo termina em `.eff`, distribuídos por 500 `.par`. O FX audit agora registra essa interseção em `feature:strip_nested_eff_frame_step` e verifica o símbolo que chega ao renderer; a contagem não prova execução em jogo.

## Original DX9 x porte GL x mudança

| Original DX9 | Porte GL antes desta etapa | Mudança desta etapa |
|---|---|---|
| `CMPModelEff::FrameMove` soma o `dt` atual ao relógio do subefeito; se alcança a duração da chave, avança **um** quadro e zera o residual (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-540`). | `legacy_eff::framePhase` escolhia a fase pela idade total e pela soma das durações (`sources/Engine/include/top/LegacyEffFormat.h:378-401`); em um `dt` que atravessasse várias chaves, podia pular chaves e conservar o excesso de tempo. | `historical_eff_timeline::Group` guarda quadro e relógio de cada filho e, por `step(dt)`, avança no máximo uma chave, zerando o residual (`sources/Engine/include/top/HistoricalEffTimeline.h:24-93`; integração em `EffectSystem.cpp:1044-1084,2050-2074`). |
| O pai STRIP cria um único slot vivo; `_FrameMoveStrip` conserva tamanho/cor/ângulo da primeira chave e chama o `CMPModelEff` aninhado (`arquivo/fontes/Engine/src/MPParticleSys.cpp:3173-3230`). | O GL usa tipo 6 com uma timeline analítica de idade; o grupo filho é calculado pelo `cycleLength` (`EffectSystem.cpp:1795-1798,2050-2066`). | Com `TOP_HISTORICAL_STRIP_EFF_FRAME_STEP=1`, a amostragem de atributos geométricos usa o estado explícito da chave por filho. O valor padrão continua desligado. |
| `CMPModelEff::FrameMove` só reinicia o grupo em loop quando nenhum controlador filho continua tocando (`MPModelEff.cpp:521-535`; `MPModelEff.h:366-374`). | O porte reiniciava o clip por idade calculada e duração máxima. | A timeline espera todos os filhos terminarem antes do reinício, seguindo a política de loop já exposta por `TOP_STRIP_NESTED_LOOP`. A associação entre cada chamada real do jogo e o argumento `Play(iTime)` segue dependente do chamador. |
| Matrizes DX9 usam vetor linha e Z-up. | O renderer GL recebe X/Z horizontais e Y-up. | R não altera posições nem matriz; somente a fase de chave. `samplePhase` interpola os valores existentes, sem copiar convenções D3D para o renderer (`LegacyEffFormat.h:403-421`). |

## Fato, inferência e desconhecido

- **Fato:** o original avança no máximo uma chave por chamada e zera `m_fCurTime` ao avançar; o porte anterior calculava a fase pela idade total.
- **Fato:** o Client desta execução tem 875 sistemas type 6 com modelo `.eff` em 500 arquivos `.par`; o ranking os associa a 242 skills. Todas as 1.031 `.par` e 1.227 `.eff` foram decodificadas pelo audit atual; `fx_decode_failures.csv` contém só o cabeçalho.
- **Inferência:** estado explícito por subefeito representa melhor o controlador DX9 quando um frame recebe `dt` maior que o tempo da chave.
- **Desconhecido/limite:** animação de UV e seleção de textura ainda usam a idade analítica; não foram convertidas em controladores por quadro. A política de loop do filho depende de como o chamador original invoca `Play(iTime)`. Não há captura visual DX9/GL.

## Ativação e reversão

```bash
TOP_HISTORICAL_STRIP_EFF_FRAME_STEP=1 \
TOP_HISTORICAL_BLEND=1 \
./build/top_gl --profile
```

Para voltar ao comportamento anterior, desligue ou omita `TOP_HISTORICAL_STRIP_EFF_FRAME_STEP`. Reinicie o processo ao comparar. Para reverter o patch: `git apply -R SK217R.patch`.

## Checklist visual DX9 x OpenGL

Use o mesmo personagem, habilidade, equipamento, posição e câmera nos dois clientes. Faça primeiro a execução DX9 e depois o GL; mantenha as demais opções iguais.

```bash
TOP_HISTORICAL_STRIP_EFF_FRAME_STEP=0 TOP_HISTORICAL_BLEND=1 ./build/top_gl --profile
TOP_HISTORICAL_STRIP_EFF_FRAME_STEP=1 TOP_HISTORICAL_BLEND=1 ./build/top_gl --profile
```

- Compare um `.eff` filho de STRIP em pelo menos dois limites de quadro; observe quando muda a geometria/interpolação.
- Compare uma atualização com atraso/frame time maior que uma duração de chave: o modo ligado deve avançar uma chave só e descartar o residual, conforme o DX9.
- Observe o reinício do filho quando todos os subefeitos terminam; repita com `TOP_STRIP_NESTED_LOOP=0` para testar a comparação sem loop.
- Registre separadamente qualquer diferença de UV/textura; esses relógios não fazem parte desta etapa.
- Confirme que Frozen Arrow, Taunt, Berserk e Illusion Slash continuam sem regressão visível. A opção fica desligada no padrão.

## Auditoria e testes

```bash
build/top_par_audit --client Client --out build/sk217r-par-audit
build/top_fx_coverage_audit --client Client --out build/sk217r-fx-coverage \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Resultados observados: `top_par_audit` encontrou 1.031 `.par`, 3.476 sistemas e 0 falhas `.par`; FX audit encontrou 1.031 `.par`, 1.227 `.eff`, 2.832 partes `.eff`, 875 filhos STRIP `.eff`, 22 lacunas, `drift=0`, `failures=0`. Build Debug completo incluiu `top_gl`; CTest passou **119/119**. Os dois testes R e o teste de regressão do FX audit passaram com ASan/UBSan (`detect_leaks=0`).

Não houve comparação visual. A etapa não declara fidelidade visual.
