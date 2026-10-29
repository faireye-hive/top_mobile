# SK2.17-E — RANGE2: reciclagem dos slots após colisão

## Resumo

Recupera a reemissão do RANGE2 depois que um slot colide, enquanto o controlador ainda permite emissões. A amostragem recompõe o estado do pool com relógio determinístico de 60 Hz, ordem dos slots e temporizador compartilhado do DX9. A mudança fica atrás de `TOP_HISTORICAL_PARTICLE_EXTENDED=1` e `TOP_HISTORICAL_RANGE2_COLLISION=1`; ambos desligados por padrão.

## Evidência: DX9, porte e mudança

| Original DX9 | Porte antes da SK2.17-E | Mudança |
|---|---|---|
| `_CreateRange2` incrementa `_fCurTime` compartilhado por chamada, zera o timer ao alcançar `_fStep`, normaliza `_vDir` e cria no slot chamado: `arquivo/fontes/Engine/src/MPParticleSys.cpp:2142-2156`. | SK2.17-D modelava uma geração por slot e não reemitia após `hit`. | `HistoricalRange2Collision::simulate` recompõe o pool completo com timer compartilhado e visitas em ordem: `sources/Engine/include/top/HistoricalRange2Collision.h:97-173`. |
| O frame move slots vivos; ao colidir, marca morto e chama `SendResMessage`; no frame seguinte, chama `_CreateRange2` se `_bStop` for falso: `arquivo/fontes/Engine/src/MPParticleSys.cpp:2218-2245,2256-2259`. | O evento de impacto era deduplicado uma vez por slot, sem geração. | Cada slot tem uma geração; hits novos disparam uma vez por geração, e eventos já processados ficam deduplicados em `sources/Engine/src/top/EffectSystem.cpp:1188-1223`. O renderer usa estado da geração ativa em `:1737-1778,1827-1834`. |
| O primeiro slot guarda o relógio do controlador; `_bStop` inicia após a vida exceder `_fLife`: `arquivo/fontes/Engine/src/MPParticleSys.cpp:2201-2204`. Slots mortos não são recriados depois que o controlador parou (`:2256-2259`). | O controlador bloqueava nascimentos após o limiar, mas não reciclava slots colididos. | A simulação interrompe emissões quando `now > controllerLife`, mantém slots vivos até colisão e deixa os mortos inativos. Teste em `sources/tests/SK217ERange2RecycleTests.cpp`. |

## Tradução OpenGL e limites

O simulador conserva X/Y horizontais e Z vertical, como no DX9. `EffectSystem` reordena para o mundo OpenGL do porte (X/altura/Y), sem importar matrizes ou estado D3D. O movimento aplica `direção normalizada * velocidade * (1/60)` por tick; o DX9 usa `_vOldPos * (_fVecl * dt)`. A diferença é a discretização nominal entre chamadas de `build`.

O timer de emissão avança em cada chamada de criação de slot morto, pois `_CreateRange2` incrementa o timer internamente e o loop visita os slots em ordem. A regra de reset é mantida, mas 60 Hz não reproduz a contagem de chamadas dependente do FPS real. A amostra do terreno ainda usa `MapFile::heightAt`; a diferença para `GetGridHeight(int(x)*2,int(y)*2)` descrita na SK2.17-D permanece.

## Auditoria e abrangência

O `fx_coverage_audit` encontrou 35 sistemas RANGE2 em `.par`, em 19 arquivos e 7 skills; todos tinham `hitEffect`. Esses são recursos referenciados, não prova de execução. A anotação tipo 15 do auditor continuava desatualizada após a SK2.17-D; esta etapa não modifica o auditor. RANGE2 permanece parcial: a reciclagem foi implementada, mas terreno de subgrade, tempo dependente de FPS e RNG global DX9 diferem.

## Testes executados

- Teste puro cobre nascimento, colisões sucessivas por geração, filtro de eventos já entregues, clamp do hit com mapa e parada de novas emissões após a vida do controlador.
- A fixture integrada da SK2.17-D foi reforçada para exigir pelo menos cinco impactos visíveis no `.par` em uma janela com cinco colisões simuladas; mantém a verificação do modo desligado.
- Build completo Debug (`cmake -S sources -B build -DCMAKE_BUILD_TYPE=Debug -DTOP_BUILD_UI_TESTS=ON`; `cmake --build build -j$(nproc)`): compilou `top_gl` e os alvos de teste com exit code 0.
- `ctest --test-dir build --output-on-failure`: **104/104 passou**. O teste novo é `ui05sk217e_range2_slot_recycling`; também passaram os testes de estado e integração SK2.17-D.
- Os três testes RANGE2 passaram com ASan/UBSan em `build/sk217d-asan`, usando `ASAN_OPTIONS=detect_leaks=0`. O LeakSanitizer padrão não funciona no ambiente sob `ptrace`, então LSan não foi verificado.
- Nenhum teste existente foi relaxado. Não houve comparação visual DX9/OpenGL.

## Comandos

```bash
cmake -S sources -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTOP_BUILD_UI_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Para comparação visual, preservando as opções anteriores:

```bash
TOP_HISTORICAL_BLEND=1 \
TOP_HISTORICAL_MODEL_TEXTURE=1 \
TOP_FROZEN_RESULT_GROUND=1 \
TOP_HISTORICAL_FILTERS=1 \
TOP_HISTORICAL_PARTICLE_POOL=1 \
TOP_HISTORICAL_PARTICLE_MOTION=1 \
TOP_HISTORICAL_PARTICLE_EXTENDED=1 \
TOP_HISTORICAL_RANGE2_COLLISION=1 \
./build/top_gl --profile
```

Desligue esta mudança com `TOP_HISTORICAL_RANGE2_COLLISION=0`. Para reverter SK2.17-E após aplicar o patch: `git apply -R SK217E.patch`.

## Fato, inferência e desconhecido

**FATO:** o original recria slots mortos enquanto `_bStop` for falso; agora a simulação recompõe gerações e os testes cobrem impactos repetidos. **INFERÊNCIA:** o relógio determinístico de 60 Hz aproxima os frames do DX9 de modo repetível. **DESCONHECIDO:** sequência exata de RNG, cadência por FPS, grade exata de terreno e aparência nas 7 skills. Não se afirma equivalência visual.
