# SK2.17-AJ — timeline dos `.eff` filhos de MODEL

## Resultado e incidência medida

O FX audit executado contra o Client atual encontrou **741 sistemas type 5**, em **384 arquivos `.par`**, com modelo aninhado `.eff`, associados a **57 skills** pelo grafo disponível. O conjunto total tem 777 sistemas MODEL em 387 arquivos. Esses vínculos são evidência estática; não demonstram execução em runtime.

Adicionei `TOP_HISTORICAL_MODEL_EFF_TIMELINE`, desligada por padrão. Ela só é consumida junto com `TOP_HISTORICAL_MODEL_FRAME_STEP=1`, para que o relógio dos filhos respeite as mesmas chamadas de atualização e o quadro de reinício do controlador MODEL. Os grupos usam `dt` real de `EffectSystem::step`; frames filhos avançam no máximo uma vez por chamada e descartam o excedente, como `CMPModelEff::FrameMove`.

## Evidência: DX9 original × porte OpenGL × mudança

| Original DX9 | Porte OpenGL antes | Mudança AJ |
|---|---|---|
| `CMPPartSys::Play` inicia MODEL e chama `CPPart->Play(!_bLoop)` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:907-922`). No caminho de `CMagicCtrl`, o `.par` recebe `Play(0)` (`MPEffectCtrl.cpp:545-548`), e `CMPModelEff::Play(0)` define loop (`arquivo/fontes/Engine/include/MPModelEff.h:377-389`). | Para os filhos de MODEL, a renderização usa fallback analítico `legacy_eff::framePhase/sample(part, modelAge)` quando o modo histórico está desligado (`sources/Engine/src/top/EffectSystem.cpp:2354-2366`). | Cada perfil type 5 tem um grupo filho separado; `modelEffTimelines` avança com o `dt` do frame e usa loop no caminho de `Play(0)` (`sources/Engine/include/top/EffectSystem.h:211`; `sources/Engine/src/top/EffectSystem.cpp:1142-1152`). |
| `_FrameMoveModel` chama `CMPModelEff::FrameMove(dt)` enquanto a partícula permanece no quadro válido; quando o pai chega ao fim do cursor, recria o MODEL e retorna sem atualizar o filho naquele passo (`MPParticleSys.cpp:3101-3110,3161-3167`). | O porte calculava o quadro/UV/textura do filho em função da idade, sem o cursor por controlador. | Com os dois switches ativos, o avanço do filho é omitido no passo em que o cursor pai sinaliza `holdingLastFrame`; fora desse ponto, `HistoricalEffTimeline::Group` atualiza quadros, UVs e textura (`EffectSystem.cpp:1142-1152,2307-2330`). |
| `CMPModelEff::FrameMove` soma `dt`, avança um quadro no máximo, zera o tempo residual e reinicia o grupo em loop quando todos os controladores filhos param (`arquivo/fontes/Engine/src/MPModelEff.cpp:500-540`). | O sample analítico podia atravessar vários quadros dentro de uma única chamada de render. | A timeline reutiliza a máquina de grupo já existente, mantendo estado independente por instância e perfil. Não foram alteradas matrizes D3D, winding, coordenadas ou geometria: a alteração é temporal; o renderer continua usando os vértices OpenGL. |

## Fatos, inferências e desconhecidos

- **FATO:** o DX9 tem cursor por controlador filho e avança-o em `CMPModelEff::FrameMove(dt)`; o `.par` pode reiniciar no passo em que o pai termina seu próprio quadro final.
- **FATO:** os dois switches permanecem opt-in. A auditoria atual mede 741 sistemas/384 `.par`/57 skills para esse mecanismo.
- **INFERÊNCIA:** em frames que excedem a duração de várias chaves, a atualização discreta deve preservar a mesma política de uma chave por chamada que o DX9; a fixture sintética reproduz essa divergência em relação à amostragem analítica.
- **DESCONHECIDO:** cadência exata do jogo DX9 durante uma sessão, execução efetiva de cada referência e paridade de pixels/transformação com o Client. Não houve comparação visual.

## `.eff` recusados e utilidade

No audit Client desta etapa, 31 arquivos foram recusados pelo parser estrito e **31/31** foram estruturalmente recuperados no modo diagnóstico. Trinta têm referência direta em PAR e ID de `sceneffectinfo.txt`; `fboomerff.eff` não tem referência direta, mas há um `fboomerff.par` de mesmo nome-base. O DX9 indexa globalmente os `.eff` (`arquivo/fontes/Engine/src/MPResManger.cpp:1351-1405`), portanto esse dado não prova que o arquivo irmão seja usado nem que seja inútil. O audit marca todos com `runtime_use_proven=no`; não descartei nenhum.

## Testes e auditoria

O teste unitário verifica switch desligado por padrão, avanço único, descarte de excedente e reinício do grupo. A fixture integrada constrói um `.par` type 5 e um filho `.eff` com três chaves e verifica que, após `dt=0.15`, o centroide do filho muda de X=15 (sample analítico) para X=10 (grupo discreto). O teste do FX audit cria um type 5 ligado a `.eff` e verifica sua contagem.

```bash
cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
build/top_fx_coverage_audit --client Client --out build/sk217aj-model-eff-timeline \
  --deps sources/auditoria-historica/dependencies.csv \
  --table sources/auditoria-historica/fx_port_coverage.csv \
  --source sources/Engine/src/top/EffectSystem.cpp
```

O build Debug completo terminou com exit code 0, incluindo `top_gl`; CTest completo passou **131/131**. Os testes unitário/integrado novos também passaram com ASan/UBSan (`detect_leaks=0`). O resultado Client observado foi: 1.031 `.par`, 1.227 `.eff`, 3.476 sistemas, 2.832 partes `.eff`, 26 lacunas, zero falhas estruturais, `drift=0` e `unmatched_strips=0`. O JSON ainda registra 31 recusas estritas; o CSV de utilidade individual confirma recuperação diagnóstica das 31, 30 referências PAR diretas e uma sem referência direta.

## Ativação, comparação e reversão

```bash
TOP_HISTORICAL_MODEL_FRAME_STEP=1 \
TOP_HISTORICAL_MODEL_EFF_TIMELINE=1 \
./build/top_gl --profile
```

Para desligar, omita os dois switches ou defina-os como `0` e reinicie o processo. Compare início, trocas de quadro e reinício dos filhos em taxas de atualização diferentes com o mesmo Client DX9. Para reverter o patch aplicado: `git apply -R SK217AJ.patch`.

Esta etapa é aproximada em relação ao comportamento estrutural descrito e **não está validada visualmente**.
