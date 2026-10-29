# SK2.17-BT — atraso discreto na timeline MODEL

## Resumo

O DX9 verifica `IsDelay()` no início de `CMPPartSys::FrameMove` e só depois atualiza o cursor MODEL. O relógio de atraso avança em `Render()`/`UpdateDelay()`, depois do update daquele quadro. O GL calculava `activeDelta` como uma fração no frame que cruzava `delayTime`, fazendo o MODEL e o `.eff` filho avançarem antes do primeiro `FrameMove` habilitado no DX9. A timeline opt-in agora descarta esse frame de cruzamento inteiro e passa o `dt` completo no update seguinte.

## Evidência DX9 × OpenGL × mudança

| Aspecto | DX9 original | OpenGL antes | Mudança |
|---|---|---|---|
| Ordem do início atrasado | `CMPPartSys::FrameMove` retorna se `IsDelay()` (`arquivo/fontes/Engine/src/MPParticleSys.cpp:1243-1257`; `IsDelay` em `arquivo/fontes/Engine/include/MPParticleSys.h:484`). O relógio é atualizado por `UpdateDelay`, chamado em `Render`, e incrementado enquanto ainda está antes do atraso (`MPParticleSys.cpp:176-189,1260-1265`). | `EffectSystem::step` calculava `max(0, newAge-delay)-max(0, oldAge-delay)`, consumindo apenas a parte do `dt` depois do atraso (`sources/Engine/src/top/EffectSystem.cpp:1193-1197`). | `historical_model_timeline::controllerDelta` retorna zero enquanto o tempo anterior ao update ainda é menor que o atraso, e retorna o `dt` inteiro a partir do update seguinte (`HistoricalModelTimeline.h:12-19`). |
| Pai MODEL e `.eff` filho | `CMPPartCtrl::FrameMove` chama cada sistema uma vez por update (`arquivo/fontes/Engine/src/MPParticleCtrl.cpp:578-583`); `_FrameMoveModel` atualiza pai e `.eff` filho nesse mesmo update (`MPParticleSys.cpp:3101-3110,3161-3167`). | O filho e o pai recebiam o `activeDelta` fracionário do primeiro frame que cruzava o atraso. | O mesmo `frameDelta` zero/completo alimenta ambas as timelines; `advance(0)` inicializa estado sem progredir quadro (`EffectSystem.cpp:1262-1274`; `HistoricalEffTimeline.h:50-70`). A timeline só é usada no modo já opt-in `TOP_HISTORICAL_MODEL_FRAME_STEP=1`, ou nos efeitos de mapa já opt-in. |
| Ordem do porte | O jogo chama `CGameScene::_FrameMove` para os efeitos durante o update (`arquivo/fontes/Client/src/SceneFrameMove.cpp:118-129`). | O GL emite eventos, chama `effects.step(dt)` e constrói o batch depois (`sources/Platform/linux/main.cpp:2747-2750,2762-2788,2937`). | O cálculo alinha a decisão à idade anterior ao `step`, sem dividir o frame que atravessa o atraso. Não alterei matriz, eixos, geometria, textura ou blend. |

## Dados e teste

O Client atual tem **114 sistemas MODEL** com `delay > 0` e mais de uma chave de curva, em **50 PARs** e associados estaticamente a **39 IDs de skills**. Desses, **103** têm `.eff` filho, em 47 skills estáticas. O FX audit agora mede `model_delayed_frame_step` a partir de `type==5 && frames>1 && delay>0`, e verifica que o renderer chama `controllerDelta`; a execução contou 114/50/39. Esses vínculos não provam execução runtime.

A fixture integrada usa um PAR v6 com atraso `0.1`, dispara `step(0.15)` e verifica que pai/filho ainda estão no começo; um segundo `step(0.1)` deve avançar o `.eff` filho. O teste unitário cobre atraso zero, update antes da fronteira, update que cruza a fronteira e update seguinte.

## Fato, inferência e desconhecido

- **FATO:** no DX9, `FrameMove` consulta `IsDelay()` antes de chamar `FrameUpdate`; o relógio é atualizado mais tarde no caminho de `Render`. Portanto, no update que começa abaixo do atraso, o `FrameUpdate` retorna sem consumir parte do `dt`.
- **FATO:** o Client contém 114 sistemas MODEL multi-quadro atrasados; o audit deriva a contagem dos arquivos PAR decodificados.
- **INFERÊNCIA:** o novo gating reproduz melhor a ordem de chamadas DX9 para esse frame do que consumir o delta fracionário.
- **DESCONHECIDO:** a cadência e ordem exatas de todos os frames do Client original durante gameplay, e paridade visual. A fixture verifica estado/centroide sintético; não compara imagens DX9 reais.

## Testes e auditoria

- Configuração Debug: `cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON` — exit 0. Build completo, incluindo `top_gl`: exit 0.
- CTest completo: **141/141 aprovados**, exit 0. Os dois testes focais unitário e integrado também passaram em Debug.
- FX audit: 1.031 PAR, 1.227 EFF, 3.476 sistemas, 2.832 partes, 27 lacunas/307 IDs estáticos, drift/falhas estruturais zero; `model_delayed_frame_step` 114/50/39. 31 EFF strict-only continuam recuperáveis em diagnóstico; 30 têm PAR direto e nenhum tem uso runtime provado.
- Testes focais unitário e integrado passaram com ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`, `UBSAN_OPTIONS=halt_on_error=1`), exit 0.

## Ativação e reversão

```bash
TOP_HISTORICAL_MODEL_FRAME_STEP=1 \
TOP_HISTORICAL_MODEL_EFF_TIMELINE=1 \
./build/top_gl --profile
```

Para comparar sem a timeline, omita ambas as variáveis ou defina-as como `0` e reinicie o processo. Efeitos de mapa continuam dependendo de `TOP_HISTORICAL_MAP_SCENE_EFFECTS=1`. Para reverter somente esta etapa depois de aplicar: `git apply -R SK217BT.patch`. O patch incremental foi verificado com `git apply --check` e aplicação/reversão em snapshot isolado dos arquivos-base da etapa; não foi aplicado novamente nesta árvore, onde as mudanças já estão presentes.
