# SK2.17-BC — auditoria de dispatch de hitEffect por tipo

## Resumo

Esta etapa corrige a classificação do campo `hitEffect` no auditor FX; não altera o renderer nem o comportamento em jogo. O campo estava agregado como uma lacuna genérica, embora o código DX9 só despache o evento em FIRE e RANGE2, e o porte já tenha caminhos condicionais para esses dois tipos. O auditor agora cruza os tipos derivados do enum DX9, `FrameUpdate`, chamadas `SendResMessage`, os ramos GL e os dados PAR, e escreve `fx_hit_effect_coverage.csv`.

## Evidência: DX9 original × OpenGL × mudança

| FATO — original DX9 | FATO — porte GL antes desta etapa | Mudança |
|---|---|---|
| `PARTTICLE_FIRE=2` e `PARTTICLE_RANGE2=15`: `arquivo/fontes/Engine/include/MPParticleSys.h:187,203`. `SetOpertion` mapeia os tipos a `_FrameMoveFire`/`_FrameMoveRange2`: `MPParticleSys.cpp:371,411`. | FIRE cria evento após cruzar os limites nos ramos em `sources/Engine/src/top/EffectSystem.cpp:1599-1635`, guardado por `TOP_HISTORICAL_FIRE_HIT_EVENT` e pool. RANGE2 consome `frameResult.hits`/`system.hits` em `:1545-1573`, guardado por `TOP_HISTORICAL_RANGE2_COLLISION` e modo estendido. | O auditor confirma tipo, função de atualização, call sites, checagem de tipo no GL, gatilho e switch antes de deixar de classificar `feature:hit_effect` como gap genérico. |
| `_FrameMoveFire` chama `SendResMessage` em `arquivo/fontes/Engine/src/MPParticleSys.cpp:3549,3562`; `_FrameMoveRange2`, em `:2235,2243`. | O campo é armazenado pelo parser GL e os dois caminhos submetem o recurso nomeado à fila; os switches seguem desligados por padrão. | O relatório por tipo diferencia dispatcher DX9 encontrado, caminho GL encontrado e gate; os tipos sem despacho explícito são identificados como campo serializado sem runtime dispatcher. |
| Os tipos MODEL (5) e STRIP (6) aparecem com `hitEffect` no Client. Na fonte DX9 inspecionada não existe um dispatcher `SendResMessage(m_strHitEff)` nesses controladores. | Esses campos permanecem lidos como dados, mas não ganham callback por inferência do formato. | O audit mantém os dados brutos e mostra explicitamente a ausência de call site DX9 para estes dois tipos. |

## Auditoria do Client

Executado pelo `top_fx_coverage_audit` no diretório `Client/` com o original DX9 e `EffectSystem.cpp` atuais. Resultado em `build/sk217bc-fx-audit/`:

- 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes EFF; 25 gaps agregados, 307 skills ligados estaticamente, drift 0 e falhas estruturais 0.
- Foram encontrados 66 campos `hitEffect`, em 46 PARs e 9 skills: FIRE (2): 5 sistemas/1 PAR; MODEL (5): 21/21; STRIP (6): 5/5; RANGE2 (15): 35/19.
- DX9 tem 4 call sites de `SendResMessage` no inventário: dois em FIRE e dois em RANGE2. O código GL tem ambos os caminhos de evento, protegidos por opções experimentais. Os CSVs de recursos registram os campos de MODEL/STRIP sem promovê-los a callbacks.
- A classificação genérica `feature:hit_effect` deixa de aparecer no ranking somente quando a verificação DX9 e GL passa. O inventário bruto continua em `fx_feature_coverage.csv` e o detalhe fica em `fx_hit_effect_coverage.csv`.

## Arquivos EFF estritos recusados

No mesmo audit, o decoder estrito recusou 31 EFF; o modo diagnóstico não registrou falhas estruturais. Trinta têm referências diretas de PAR. `fboomerff.eff` não tem referência direta, mas há `fboomerff.par` homônimo catalogado em cena 919/skill 308, sem vínculo direto entre seus sistemas e o EFF. O DX9 enumera, carrega e indexa o catálogo de EFF (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`). Isso prova inclusão no catálogo e recuperabilidade diagnóstica, não execução/renderização. Portanto nenhum dos 31 foi descartado; o uso em runtime de `fboomerff.eff` continua desconhecido.

## Fato, inferência e desconhecido

- **FATO:** a fonte DX9 contém os dois dispatchers de evento, apenas para FIRE e RANGE2 entre os tipos encontrados com campo não vazio; o porte contém caminhos de evento para esses dois tipos sob switches opt-in.
- **INFERÊNCIA:** a correção do ranking reduz falso positivo de cobertura porque ele agora separa “dado serializado” de “evento com dispatcher comprovado”. Isso não mede frequência de execução em sessão real.
- **DESCONHECIDO:** se cada skill com o campo não vazio efetivamente atinge o ramo de colisão; se os recursos nomeados de impacto existem e são carregados no momento; e a equivalência visual dos efeitos disparados.

## Testes e execução

- Fixture sintética `.par` v11 cobre tipos 2, 5, 6 e 15, inclui nomes de hit effect e testa o vínculo do enum/dispatcher DX9 com os caminhos GL. Um scanner de tipo que confundia a lista `profile.type==14 || profile.type==15` foi corrigido e a fixture reprova novamente se o path RANGE2 não for reconhecido.
- `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON`: sucesso.
- `cmake --build build -j$(nproc)`: sucesso, incluindo `top_gl`.
- `ctest --test-dir build --output-on-failure`: **138/138 passou**.
- A fixture focal também passou com ASan/UBSan, usando `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1`. LSan não foi executado. Não houve comparação visual com DX9; esta mudança é do auditor e não declara equivalência visual.

Repetir a auditoria dos recursos e comparar com os call sites atuais:

```bash
build/top_fx_coverage_audit \
  --client Client --out build/sk217bc-fx-audit \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp \
  --dx9-source arquivo/fontes/Engine/src/MPResManger.cpp \
  --dx9-particle-source arquivo/fontes/Engine/src/MPParticleSys.cpp \
  --dx9-particle-header arquivo/fontes/Engine/include/MPParticleSys.h \
  --controller-strips sources/auditoria-historica/DIRECT_CONTROLLER_STRIPS_SK212.csv
```

## Reversão e próximo passo

Não há variável nova de runtime nem alteração do renderer. O recurso existente FIRE continua sob `TOP_HISTORICAL_FIRE_HIT_EVENT=1`; RANGE2 continua sob `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e `TOP_HISTORICAL_RANGE2_COLLISION=1`, ambos desligados por padrão. Após aplicar o patch, reverta com `git apply -R SK217BC.patch`.

Próximo passo: selecionar uma lacuna de runtime do ranking atual e comparar seu call site DX9 com a implementação GL antes de alterar comportamento. Os efeitos RANGE2 (35 sistemas/7 skills) têm os caminhos de callback medidos, mas a equivalência visual e de terreno continua pendente.
